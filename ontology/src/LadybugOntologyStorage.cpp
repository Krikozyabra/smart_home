#include "ontology/LadybugOntologyStorage.h"
#include "SnapshotFiles.h"
#include "logging/Logging.h"
#include <lbug.hpp>
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <variant>

namespace smart_home::ontology {
namespace fs = std::filesystem;
namespace {
using lbug::main::QueryResult;
using lbug::common::Value;
using Cell = std::variant<std::string, std::int64_t>;
using Params = std::initializer_list<std::pair<std::string, Cell>>;
using Tuple = lbug::processor::FlatTuple;

void nonempty(const std::string& value) {
    if (value.empty() || value.find('\0') != std::string::npos)
        throw std::invalid_argument("Ontology strings must be nonempty and contain no NUL.");
}
void validId(DeviceId id) {
    if (id < 0) throw std::invalid_argument("Ontology device ID must be nonnegative.");
}
std::string text(Tuple& row, std::size_t column) {
    const auto* value = row.getValue(column);
    if (value->isNull()) throw std::runtime_error("Incompatible ontology: null record property.");
    return value->getValue<std::string>();
}
template<class Record> Record record(Tuple& row);
template<> RoomRecord record(Tuple& row) { return {text(row, 0), text(row, 1)}; }
template<> OntologyDeviceRecord record(Tuple& row) {
    return {row.getValue(0)->getValue<std::int64_t>(), text(row, 1)};
}
template<> AbilityRecord record(Tuple& row) { return {text(row, 0), text(row, 1), text(row, 2)}; }
} // namespace

class LadybugOntologyStorage::Impl {
public:
    explicit Impl(OntologyConfig input) : config(std::move(input)) {
        validateConfig(config);
        try {
            durable = detail::canonicalDestination(config.persistent_path);
            lock.reset(detail::acquireLock(durable));
            detail::removeStaleSnapshots(durable);
            const auto existingSize = fs::exists(durable) ? fs::file_size(durable) : 0;
            if (fs::exists(durable) && existingSize == 0)
                throw std::runtime_error("Existing ontology database is empty or corrupt.");
            // Reserve room for the source, checkpoint growth, WAL and shadow files.
            const auto reserve = std::max<std::uintmax_t>(64 * 1024 * 1024, existingSize * 3);
            try {
                workspace = detail::makeWorkspace(config.ram_workspace_base, reserve);
            } catch (...) {
                if (!config.allow_disk_fallback) throw;
                workspace = detail::makeWorkspace(durable.parent_path(), reserve);
                logging::log(logging::Level::Warn, "Ontology", "RAM workspace unavailable; using explicitly enabled disk workspace.");
            }
            runtime = workspace / "workspace.lbdb";
            if (fs::exists(durable)) {
                // Only the engine can recover sidecars. Never discard a WAL or
                // take a .lbdb-only copy of an interrupted database.
                if (hasRecoveryFiles()) {
                    openEngine(durable);
                    query("CHECKPOINT");
                    closeEngine();
                }
                fs::copy_file(durable, runtime);
            } else if (hasRecoveryFiles()) {
                throw std::runtime_error("Ontology recovery sidecars exist without a main database.");
            }
            openEngine(runtime);
            schema(!fs::exists(durable));
            if (!fs::exists(durable)) {
                dirty = true;
                syncLocked(true);
            }
            if (config.sync_interval.count() > 0) worker = std::thread([this] { workerLoop(); });
        } catch (...) {
            closeEngine();
            cleanupWorkspace();
            throw;
        }
    }

    ~Impl() noexcept {
        try { close(); }
        catch (...) {
            logging::log(logging::Level::Critical, "Ontology", "Final snapshot failed; RAM workspace retained for manual recovery.");
            closeEngine();
            // Retain files on failure; the lock descriptor is released by RAII.
        }
    }

    void requireOpen() const {
        if (closed) throw std::logic_error("Ontology storage is closed.");
        if (!connection) throw std::logic_error("Ontology engine unavailable after failure; close and recover retained workspace.");
    }

    std::unique_ptr<QueryResult> query(const std::string& cypher, Params params = {}) const {
        std::unique_ptr<QueryResult> result;
        if (params.size() == 0) result = connection->query(cypher);
        else {
            auto statement = connection->prepare(cypher);
            if (!statement->isSuccess()) throw std::runtime_error("Ontology prepare failed: " + statement->getErrorMessage());
            std::unordered_map<std::string, std::unique_ptr<Value>> values;
            for (const auto& [name, value] : params)
                std::visit([&](const auto& item) { values.emplace(name, std::make_unique<Value>(item)); }, value);
            result = connection->executeWithParams(statement.get(), std::move(values));
        }
        if (!result || !result->isSuccess())
            throw std::runtime_error("Ontology query failed: " + (result ? result->getErrorMessage() : "no result"));
        return result;
    }

    template<class Record>
    std::vector<Record> records(const std::string& cypher, Params params = {}) const {
        std::vector<Record> output;
        auto result = query(cypher, params);
        while (result->hasNext()) output.push_back(record<Record>(*result->getNext()));
        return output; // FlatTuple is reused by Ladybug; all values above are owned copies.
    }

    bool exists(const std::string& table, const Cell& id) const {
        return query("MATCH (n:" + table + ") WHERE n.id = $id RETURN n.id", {{"id", id}})->hasNext();
    }
    void endpoint(const std::string& table, const Cell& id) const {
        if (!exists(table, id)) throw std::invalid_argument("Ontology relationship endpoint does not exist.");
    }

    template<class Operation> void transaction(Operation operation) {
        query("BEGIN TRANSACTION");
        try {
            operation();
            query("COMMIT");
            dirty = true;
        } catch (...) {
            const auto failure = std::current_exception();
            try { query("ROLLBACK"); }
            catch (...) { closeEngine(); }
            std::rethrow_exception(failure);
        }
    }

    void sync() {
        std::lock_guard<std::mutex> guard(transactionMutex);
        requireOpen();
        syncLocked(true);
    }

    void close() {
        std::lock_guard<std::mutex> lifecycleGuard(closeMutex);
        {
            std::lock_guard<std::mutex> guard(workerMutex);
            stop = true;
        }
        workerCondition.notify_all();
        if (worker.joinable()) worker.join(); // Never join while holding transactionMutex.
        std::lock_guard<std::mutex> guard(transactionMutex);
        if (closed) return;
        // A failed reopen still leaves a checkpointed RAM file which close can
        // save. No subsequent query is accepted in that state.
        syncLocked(false);
        closeEngine();
        closed = true;
        cleanupWorkspace();
        lock.reset();
    }

    mutable std::mutex transactionMutex;
    detail::SnapshotHook snapshotHook;
    fs::path runtime;
    std::mutex workerMutex;
    std::condition_variable workerCondition;
    bool requested = false;
    std::size_t attempts = 0;
    std::thread worker;

private:
    lbug::main::SystemConfig engineConfig() const {
        lbug::main::SystemConfig settings;
        settings.bufferPoolSize = config.max_buffer_pool_bytes;
        settings.maxNumThreads = config.max_threads;
        settings.autoCheckpoint = false;
        settings.forceCheckpointOnClose = true;
        settings.throwOnWalReplayFailure = true;
        return settings;
    }
    void openEngine(const fs::path& path) {
        database = std::make_unique<lbug::main::Database>(path.string(), engineConfig());
        try { connection = std::make_unique<lbug::main::Connection>(database.get()); }
        catch (...) { database.reset(); throw; }
    }
    void closeEngine() noexcept {
        connection.reset();
        database.reset();
    }
    bool hasRecoveryFiles() const {
        for (const char* suffix : {".wal", ".shadow", ".wal.checkpoint"})
            if (fs::exists(durable.string() + suffix)) return true;
        return false;
    }
    void cleanupWorkspace() noexcept {
        if (workspace.empty()) return;
        std::error_code error;
        fs::remove_all(workspace, error);
        if (error) logging::log(logging::Level::Warn, "Ontology", "Cannot remove ontology workspace.");
    }

    void schema(bool fresh) {
        if (fresh) {
            transaction([&] {
                query("CREATE NODE TABLE rooms(id STRING, name STRING, PRIMARY KEY(id))");
                query("CREATE NODE TABLE devices(id INT64, name STRING, PRIMARY KEY(id))");
                query("CREATE NODE TABLE abilities(id STRING, name STRING, category STRING, PRIMARY KEY(id))");
                query("CREATE REL TABLE LOCATED_IN(FROM devices TO rooms)");
                query("CREATE REL TABLE HAS_FEATURE(FROM devices TO abilities)");
            });
        }
        std::map<std::string, std::string> tables;
        auto result = query("CALL show_tables() RETURN name, type");
        while (result->hasNext()) {
            auto row = result->getNext();
            tables.emplace(text(*row, 0), text(*row, 1));
        }
        const std::map<std::string, std::string> expected{
            {"rooms", "NODE"}, {"devices", "NODE"}, {"abilities", "NODE"},
            {"LOCATED_IN", "REL"}, {"HAS_FEATURE", "REL"}};
        if (tables != expected) throw std::runtime_error("Incompatible ontology tables; explicit migration required.");
        for (const auto& table : {std::string("rooms"), std::string("devices"), std::string("abilities")}) {
            auto info = query("CALL table_info('" + table + "') RETURN name, type, `primary key`");
            std::map<std::string, std::pair<std::string, bool>> properties;
            while (info->hasNext()) {
                auto row = info->getNext();
                properties.emplace(text(*row, 0), std::make_pair(text(*row, 1), row->getValue(2)->getValue<bool>()));
            }
            std::map<std::string, std::pair<std::string, bool>> wanted{
                {"id", {table == "devices" ? "INT64" : "STRING", true}}, {"name", {"STRING", false}}};
            if (table == "abilities") wanted.emplace("category", std::make_pair("STRING", false));
            if (properties != wanted) throw std::runtime_error("Incompatible ontology properties; explicit migration required.");
        }
        for (const auto& [relation, target] : {std::pair{"LOCATED_IN", "rooms"}, std::pair{"HAS_FEATURE", "abilities"}}) {
            auto info = query(std::string("CALL show_connection('") + relation + "') RETURN *");
            if (!info->hasNext()) throw std::runtime_error("Missing ontology relationship endpoints.");
            auto row = info->getNext();
            if (text(*row, 0) != "devices" || text(*row, 1) != target ||
                text(*row, 2) != "id" || text(*row, 3) != "id" || info->hasNext())
                throw std::runtime_error("Incompatible ontology relationship endpoints.");
        }
        if (query("MATCH (d:devices)-[r:LOCATED_IN]->(:rooms) WITH d.id AS id, count(r) AS c WHERE c > 1 RETURN id")->hasNext() ||
            query("MATCH (d:devices)-[r:HAS_FEATURE]->(a:abilities) WITH d.id AS id, a.id AS aid, count(r) AS c WHERE c > 1 RETURN id")->hasNext())
            throw std::runtime_error("Incompatible ontology: duplicate or ambiguous relationships.");
    }

    void syncLocked(bool reopen) {
        if (!dirty) return;
        if (connection) {
            query("CHECKPOINT");
            closeEngine(); // No connection, transaction or background engine work can change the snapshot.
        }
        std::exception_ptr failure;
        try {
            detail::atomicSnapshot(runtime, durable, snapshotHook);
            dirty = false;
        } catch (...) { failure = std::current_exception(); }
        if (reopen || failure) {
            try {
                if (snapshotHook) snapshotHook(detail::SnapshotStage::BeforeReopen);
                openEngine(runtime);
            } catch (...) {
                if (!failure) failure = std::current_exception();
                // Keep the original I/O failure if both snapshot and reopen fail.
            }
        }
        if (failure) std::rethrow_exception(failure);
    }

    void workerLoop() noexcept {
        std::unique_lock<std::mutex> guard(workerMutex);
        while (!stop) {
            workerCondition.wait_for(guard, config.sync_interval, [&] { return stop || requested; });
            if (stop) break;
            requested = false;
            guard.unlock();
            try { sync(); }
            catch (...) { logging::log(logging::Level::Error, "Ontology", "Periodic snapshot failed; retrying at next interval."); }
            guard.lock();
            ++attempts;
            workerCondition.notify_all();
        }
    }

    OntologyConfig config;
    fs::path durable;
    fs::path workspace;
    detail::FileDescriptor lock;
    std::unique_ptr<lbug::main::Database> database;
    std::unique_ptr<lbug::main::Connection> connection;
    std::mutex closeMutex;
    bool stop = false;
    bool closed = false;
    bool dirty = false;
};

LadybugOntologyStorage::LadybugOntologyStorage(OntologyConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
LadybugOntologyStorage::~LadybugOntologyStorage() noexcept = default;
void LadybugOntologyStorage::sync() { impl_->sync(); }
void LadybugOntologyStorage::close() { impl_->close(); }

// Every public operation owns the transaction mutex until all result values have
// been copied, including multi-query reads such as getDeviceDetails().
#define ONTOLOGY_GUARD std::lock_guard<std::mutex> guard(impl_->transactionMutex); impl_->requireOpen()

void LadybugOntologyStorage::assignDeviceToRoom(DeviceId device_id, const std::string& room_id) {
    ONTOLOGY_GUARD;
    validId(device_id); nonempty(room_id);
    impl_->endpoint("devices", device_id); impl_->endpoint("rooms", room_id);
    impl_->transaction([&] {
        impl_->query("MATCH (d:devices)-[r:LOCATED_IN]->(:rooms) WHERE d.id = $id DELETE r", {{"id", device_id}});
        impl_->query("MATCH (d:devices), (r:rooms) WHERE d.id = $id AND r.id = $room CREATE (d)-[:LOCATED_IN]->(r)",
                     {{"id", device_id}, {"room", room_id}});
    });
}
void LadybugOntologyStorage::removeDeviceFromRoom(DeviceId device_id) {
    ONTOLOGY_GUARD;
    validId(device_id); impl_->endpoint("devices", device_id);
    impl_->transaction([&] {
        impl_->query("MATCH (d:devices)-[r:LOCATED_IN]->(:rooms) WHERE d.id = $id DELETE r", {{"id", device_id}});
    });
}
std::optional<RoomRecord> LadybugOntologyStorage::getDeviceRoom(DeviceId device_id) const {
    ONTOLOGY_GUARD;
    validId(device_id); impl_->endpoint("devices", device_id);
    auto values = impl_->records<RoomRecord>("MATCH (d:devices)-[:LOCATED_IN]->(r:rooms) WHERE d.id = $id RETURN r.id, r.name", {{"id", device_id}});
    if (values.empty()) return std::nullopt;
    return values.front();
}
std::vector<OntologyDeviceRecord> LadybugOntologyStorage::getDevicesInRoom(const std::string& room_id) const {
    ONTOLOGY_GUARD;
    nonempty(room_id); impl_->endpoint("rooms", room_id);
    return impl_->records<OntologyDeviceRecord>("MATCH (d:devices)-[:LOCATED_IN]->(r:rooms) WHERE r.id = $id RETURN d.id, d.name ORDER BY d.id", {{"id", room_id}});
}
void LadybugOntologyStorage::addDeviceAbility(DeviceId device_id, const std::string& ability_id) {
    ONTOLOGY_GUARD;
    validId(device_id); nonempty(ability_id);
    impl_->endpoint("devices", device_id); impl_->endpoint("abilities", ability_id);
    impl_->transaction([&] {
        impl_->query("MATCH (d:devices), (a:abilities) WHERE d.id = $id AND a.id = $ability MERGE (d)-[:HAS_FEATURE]->(a)",
                     {{"id", device_id}, {"ability", ability_id}});
    });
}
void LadybugOntologyStorage::removeDeviceAbility(DeviceId device_id, const std::string& ability_id) {
    ONTOLOGY_GUARD;
    validId(device_id); nonempty(ability_id);
    impl_->endpoint("devices", device_id); impl_->endpoint("abilities", ability_id);
    impl_->transaction([&] {
        impl_->query("MATCH (d:devices)-[r:HAS_FEATURE]->(a:abilities) WHERE d.id = $id AND a.id = $ability DELETE r",
                     {{"id", device_id}, {"ability", ability_id}});
    });
}
std::vector<AbilityRecord> LadybugOntologyStorage::getDeviceAbilities(DeviceId device_id) const {
    ONTOLOGY_GUARD;
    validId(device_id); impl_->endpoint("devices", device_id);
    return impl_->records<AbilityRecord>("MATCH (d:devices)-[:HAS_FEATURE]->(a:abilities) WHERE d.id = $id RETURN a.id, a.name, a.category ORDER BY a.id", {{"id", device_id}});
}
std::vector<OntologyDeviceRecord> LadybugOntologyStorage::getDevicesWithAbility(const std::string& ability_id) const {
    ONTOLOGY_GUARD;
    nonempty(ability_id); impl_->endpoint("abilities", ability_id);
    return impl_->records<OntologyDeviceRecord>("MATCH (d:devices)-[:HAS_FEATURE]->(a:abilities) WHERE a.id = $id RETURN d.id, d.name ORDER BY d.id", {{"id", ability_id}});
}
std::optional<DeviceOntologyDetails> LadybugOntologyStorage::getDeviceDetails(DeviceId device_id) const {
    ONTOLOGY_GUARD;
    validId(device_id);
    const auto devices = impl_->records<OntologyDeviceRecord>("MATCH (d:devices) WHERE d.id = $id RETURN d.id, d.name", {{"id", device_id}});
    if (devices.empty()) return std::nullopt;
    DeviceOntologyDetails details{devices.front(), {}, {}};
    auto rooms = impl_->records<RoomRecord>("MATCH (d:devices)-[:LOCATED_IN]->(r:rooms) WHERE d.id = $id RETURN r.id, r.name", {{"id", device_id}});
    if (!rooms.empty()) details.room_id = rooms.front().id;
    auto abilities = impl_->records<AbilityRecord>("MATCH (d:devices)-[:HAS_FEATURE]->(a:abilities) WHERE d.id = $id RETURN a.id, a.name, a.category ORDER BY a.id", {{"id", device_id}});
    for (const auto& ability : abilities) details.ability_ids.push_back(ability.id);
    return details;
}

namespace detail {
void OntologyTestAccess::setSnapshotHook(LadybugOntologyStorage& storage, SnapshotHook hook) {
    std::lock_guard<std::mutex> guard(storage.impl_->transactionMutex);
    storage.impl_->snapshotHook = std::move(hook);
}
fs::path OntologyTestAccess::runtimePath(const LadybugOntologyStorage& storage) {
    std::lock_guard<std::mutex> guard(storage.impl_->transactionMutex);
    return storage.impl_->runtime;
}
namespace {
template<class Impl> void waitForAttempt(Impl& impl, bool trigger) {
    std::unique_lock<std::mutex> guard(impl.workerMutex);
    if (!impl.worker.joinable()) throw std::logic_error("Periodic worker is not running.");
    const auto next = impl.attempts + 1;
    if (trigger) { impl.requested = true; impl.workerCondition.notify_all(); }
    if (!impl.workerCondition.wait_for(guard, std::chrono::seconds(10), [&] { return impl.attempts >= next; }))
        throw std::runtime_error("Periodic sync timed out.");
}
} // namespace
void OntologyTestAccess::triggerPeriodicSync(LadybugOntologyStorage& storage) { waitForAttempt(*storage.impl_, true); }
void OntologyTestAccess::waitForPeriodicSync(LadybugOntologyStorage& storage) { waitForAttempt(*storage.impl_, false); }
} // namespace detail

// Node CRUD implementations follow below.

void LadybugOntologyStorage::addRoom(const RoomRecord& room) {
    ONTOLOGY_GUARD;
    nonempty(room.id); nonempty(room.name);
    impl_->transaction([&] {
        impl_->query("MERGE (n:rooms {id: $id}) SET n.name = $name", {{"id", room.id}, {"name", room.name}});
    });
}
std::optional<RoomRecord> LadybugOntologyStorage::getRoom(const std::string& room_id) const {
    ONTOLOGY_GUARD;
    nonempty(room_id);
    auto values = impl_->records<RoomRecord>("MATCH (n:rooms) WHERE n.id = $id RETURN n.id, n.name", {{"id", room_id}});
    if (values.empty()) return std::nullopt;
    return values.front();
}
std::vector<RoomRecord> LadybugOntologyStorage::getAllRooms() const {
    ONTOLOGY_GUARD;
    return impl_->records<RoomRecord>("MATCH (n:rooms) RETURN n.id, n.name ORDER BY n.id");
}
bool LadybugOntologyStorage::deleteRoom(const std::string& room_id) {
    ONTOLOGY_GUARD;
    nonempty(room_id);
    if (!impl_->exists("rooms", room_id)) return false;
    impl_->transaction([&] {
        impl_->query("MATCH (n:rooms) WHERE n.id = $id DETACH DELETE n", {{"id", room_id}});
    });
    return true;
}

void LadybugOntologyStorage::addDevice(const OntologyDeviceRecord& device) {
    ONTOLOGY_GUARD;
    validId(device.id); nonempty(device.name);
    impl_->transaction([&] {
        impl_->query("MERGE (n:devices {id: $id}) SET n.name = $name", {{"id", device.id}, {"name", device.name}});
    });
}
std::optional<OntologyDeviceRecord> LadybugOntologyStorage::getDevice(DeviceId id) const {
    ONTOLOGY_GUARD;
    validId(id);
    auto values = impl_->records<OntologyDeviceRecord>("MATCH (n:devices) WHERE n.id = $id RETURN n.id, n.name", {{"id", id}});
    if (values.empty()) return std::nullopt;
    return values.front();
}
std::vector<OntologyDeviceRecord> LadybugOntologyStorage::getAllDevices() const {
    ONTOLOGY_GUARD;
    return impl_->records<OntologyDeviceRecord>("MATCH (n:devices) RETURN n.id, n.name ORDER BY n.id");
}
bool LadybugOntologyStorage::deleteDevice(DeviceId id) {
    ONTOLOGY_GUARD;
    validId(id);
    if (!impl_->exists("devices", id)) return false;
    impl_->transaction([&] {
        impl_->query("MATCH (n:devices) WHERE n.id = $id DETACH DELETE n", {{"id", id}});
    });
    return true;
}

void LadybugOntologyStorage::addAbility(const AbilityRecord& ability) {
    ONTOLOGY_GUARD;
    nonempty(ability.id); nonempty(ability.name); nonempty(ability.category);
    impl_->transaction([&] {
        impl_->query("MERGE (n:abilities {id: $id}) SET n.name = $name, n.category = $extra", {{"id", ability.id}, {"name", ability.name}, {"extra", ability.category}});
    });
}
std::optional<AbilityRecord> LadybugOntologyStorage::getAbility(const std::string& ability_id) const {
    ONTOLOGY_GUARD;
    nonempty(ability_id);
    auto values = impl_->records<AbilityRecord>("MATCH (n:abilities) WHERE n.id = $id RETURN n.id, n.name, n.category", {{"id", ability_id}});
    if (values.empty()) return std::nullopt;
    return values.front();
}
std::vector<AbilityRecord> LadybugOntologyStorage::getAllAbilities() const {
    ONTOLOGY_GUARD;
    return impl_->records<AbilityRecord>("MATCH (n:abilities) RETURN n.id, n.name, n.category ORDER BY n.id");
}
bool LadybugOntologyStorage::deleteAbility(const std::string& ability_id) {
    ONTOLOGY_GUARD;
    nonempty(ability_id);
    if (!impl_->exists("abilities", ability_id)) return false;
    impl_->transaction([&] {
        impl_->query("MATCH (n:abilities) WHERE n.id = $id DETACH DELETE n", {{"id", ability_id}});
    });
    return true;
}

#undef ONTOLOGY_GUARD
} // namespace smart_home::ontology
