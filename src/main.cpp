/** @file main.cpp
 * @brief Композиция приложения, консольный цикл и фиксация полных поколений.
 */
#include "command/Command.h"
#include "device_management/DeviceManager.h"
#include "discovery/DiscoveryService.h"
#include "discovery/SimulatedDiscoverySource.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "execution/CommandDispatcher.h"
#include "logging/Logging.h"
#include "ontology/LadybugOntologyStorage.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/SQLiteDeviceStorage.h"

#include <fcntl.h>
#include <linux/magic.h>
#include <openssl/evp.h>
#include <poll.h>
#include <signal.h>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/statfs.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace {
using namespace smart_home;
namespace fs = std::filesystem;
namespace log = smart_home::logging;
using Ontology = ontology::LadybugOntologyStorage;
using Revision = std::int64_t;

[[noreturn]] void systemError(const char *operation) {
    throw std::runtime_error(std::string(operation) + ": " +
                             std::strerror(errno));
}

class Fd {
    int value_ = -1;

  public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() {
        if (value_ >= 0)
            ::close(value_);
    }
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
    int get() const { return value_; }
};

class EventPipe {
    Fd reader_, writer_;
    static std::array<int, 2> openPipe() {
        std::array<int, 2> fds{};
        if (::pipe2(fds.data(), O_NONBLOCK | O_CLOEXEC) != 0)
            systemError("pipe2");
        return fds;
    }
    explicit EventPipe(std::array<int, 2> fds)
        : reader_(fds[0]), writer_(fds[1]) {}

  public:
    EventPipe() : EventPipe(openPipe()) {}
    int reader() const { return reader_.get(); }
    int writer() const { return writer_.get(); }
    void notify() const noexcept {
        const char byte = 1;
        while (::write(writer(), &byte, 1) < 0 && errno == EINTR) {
        }
    }
    void drain() const {
        char bytes[128];
        while (::read(reader(), bytes, sizeof(bytes)) > 0) {
        }
    }
};

static_assert(std::atomic<bool>::is_always_lock_free &&
                  std::atomic<int>::is_always_lock_free,
              "Signal notification requires lock-free atomics.");
std::atomic<bool> stop_requested{false};
std::atomic<int> signal_writer{-1};
extern "C" void requestStop(int) {
    const int saved_errno = errno;
    stop_requested = 1;
    const char byte = 1;
    const int writer = signal_writer.load(std::memory_order_relaxed);
    if (writer >= 0) {
        const auto ignored = ::write(writer, &byte, 1);
        (void)ignored;
    }
    errno = saved_errno;
}

class Signals {
    struct sigaction old_int_{}, old_term_{};

  public:
    explicit Signals(const EventPipe &pipe) {
        stop_requested = 0;
        signal_writer = pipe.writer();
        struct sigaction action{};
        action.sa_handler = requestStop;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGINT, &action, &old_int_) != 0) {
            signal_writer = -1;
            systemError("sigaction");
        }
        if (sigaction(SIGTERM, &action, &old_term_) != 0) {
            sigaction(SIGINT, &old_int_, nullptr);
            signal_writer = -1;
            systemError("sigaction");
        }
    }
    ~Signals() {
        sigaction(SIGINT, &old_int_, nullptr);
        sigaction(SIGTERM, &old_term_, nullptr);
        signal_writer = -1;
    }
};

class StdinFlags {
    int old_;

  public:
    StdinFlags() : old_(fcntl(STDIN_FILENO, F_GETFL)) {
        if (old_ < 0 || fcntl(STDIN_FILENO, F_SETFL, old_ | O_NONBLOCK) < 0)
            systemError("stdin flags");
    }
    ~StdinFlags() { fcntl(STDIN_FILENO, F_SETFL, old_); }
};

template <class T> T integer(const std::string &text) {
    T result{};
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Expected a complete integer in range.");
    return result;
}

struct Config {
    fs::path state = "data/runtime";
    fs::path ram = "/dev/shm";
    std::size_t capacity = 4;
};

Config arguments(int argc, char *argv[],
                 std::vector<const char *> &logging_arguments) {
    Config config;
    if (const char *value = std::getenv("SMART_HOME_RUNTIME_STATE_DIR"))
        config.state = value;
    if (const char *value = std::getenv("SMART_HOME_RUNTIME_RAM_DIR"))
        config.ram = value;
    if (const char *value = std::getenv("SMART_HOME_RUNTIME_SNAPSHOT_CAPACITY"))
        config.capacity = integer<std::size_t>(value);
    // The standalone ontology destination/worker must never bypass CURRENT.
    for (const char *name :
         {"SMART_HOME_ONTOLOGY_PATH", "SMART_HOME_ONTOLOGY_RAM_DIR",
          "SMART_HOME_ONTOLOGY_SYNC_SECONDS",
          "SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK"})
        if (std::getenv(name))
            throw std::invalid_argument(
                "Standalone ontology settings conflict with runtime; use "
                "--runtime-* settings.");
    for (int i = 1; i < argc; ++i) {
        std::string argument(argv[i]);
        if (argument.rfind("--ontology-", 0) == 0)
            throw std::invalid_argument(
                "Standalone ontology settings conflict with runtime.");
        if (argument.rfind("--runtime-", 0) != 0) {
            logging_arguments.push_back(argv[i]);
            continue;
        }
        const auto equal = argument.find('=');
        const auto name = argument.substr(0, equal);
        if (name != "--runtime-state-dir" && name != "--runtime-ram-dir" &&
            name != "--runtime-snapshot-capacity")
            throw std::invalid_argument("Unknown runtime option.");
        std::string value;
        if (equal != std::string::npos)
            value = argument.substr(equal + 1);
        else {
            if (++i >= argc)
                throw std::invalid_argument("Missing runtime option value.");
            value = argv[i];
        }
        if (name == "--runtime-state-dir")
            config.state = value;
        if (name == "--runtime-ram-dir")
            config.ram = value;
        if (name == "--runtime-snapshot-capacity")
            config.capacity = integer<std::size_t>(value);
    }
    if (config.state.empty() || config.ram.empty() || config.capacity == 0)
        throw std::invalid_argument(
            "Runtime paths and snapshot capacity must be nonempty/positive.");
    return config;
}

// mkdtemp owns private files even when construction of a later service fails.
class Directory {
  public:
    fs::path path;
    bool preserve = false;
    Directory(const fs::path &base, const std::string &prefix) {
        auto pattern = (base / (prefix + "XXXXXX")).string();
        if (!::mkdtemp(pattern.data()))
            systemError("mkdtemp");
        path = pattern;
    }
    ~Directory() {
        if (!preserve) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }
    Directory(const Directory &) = delete;
    Directory &operator=(const Directory &) = delete;
};

void syncPath(const fs::path &path, bool directory = false);

void createStateDirectories(const fs::path &path) {
    std::vector<fs::path> missing;
    for (auto next = fs::absolute(path); !fs::exists(next);
         next = next.parent_path())
        missing.push_back(next);
    fs::create_directories(path);
    for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
        syncPath(*it, true);
        syncPath(it->parent_path(), true);
    }
}

class StateLock {
    Fd fd_;
    static int acquire(const fs::path &state) {
        createStateDirectories(state / "generations");
        if (!fs::is_directory(fs::symlink_status(state / "generations")))
            throw std::runtime_error(
                "Generation directory must not be a symlink.");
        Fd fd(::open((state / "runtime.lock").c_str(),
                     O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600));
        if (fd.get() < 0)
            systemError("open runtime lock");
        if (::flock(fd.get(), LOCK_EX | LOCK_NB) != 0)
            throw std::runtime_error(
                "Runtime state is locked by another process.");
        int result = ::fcntl(fd.get(), F_DUPFD_CLOEXEC, 0);
        if (result < 0)
            systemError("dup runtime lock");
        return result;
    }

  public:
    explicit StateLock(const fs::path &state) : fd_(acquire(state)) {}
};

void requireRam(const fs::path &path) {
    struct statfs info{};
    if (::statfs(path.c_str(), &info) != 0)
        systemError("statfs RAM directory");
    if (info.f_type != TMPFS_MAGIC && info.f_type != RAMFS_MAGIC)
        throw std::runtime_error("Runtime workspace must be on tmpfs/ramfs; "
                                 "disk fallback is disabled.");
}

void syncPath(const fs::path &path, bool directory) {
    Fd fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW |
                                   (directory ? O_DIRECTORY : 0)));
    if (fd.get() < 0)
        systemError("open for fsync");
    while (::fsync(fd.get()) != 0) {
        if (errno != EINTR)
            systemError("fsync");
    }
}

void writeFile(const fs::path &path, const std::string &content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << content;
    file.close();
}

std::string readFile(const fs::path &path) {
    if (!fs::is_regular_file(fs::symlink_status(path)) ||
        fs::file_size(path) > 4096)
        throw std::runtime_error("Invalid runtime manifest file.");
    std::ifstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit);
    if (!file)
        throw std::runtime_error("Cannot read runtime manifest.");
    return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
}

std::string digest(const fs::path &path) {
    if (!fs::is_regular_file(fs::symlink_status(path)))
        throw std::runtime_error("Missing generation file.");
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("SHA256 initialization failed.");
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read generation file.");
    std::array<char, 65536> buffer;
    while (file) {
        file.read(buffer.data(), buffer.size());
        if (EVP_DigestUpdate(ctx.get(), buffer.data(),
                             static_cast<std::size_t>(file.gcount())) != 1)
            throw std::runtime_error("SHA256 update failed.");
    }
    if (!file.eof())
        throw std::runtime_error("Generation read failed.");
    unsigned char bytes[EVP_MAX_MD_SIZE];
    unsigned length = 0;
    if (EVP_DigestFinal_ex(ctx.get(), bytes, &length) != 1)
        throw std::runtime_error("SHA256 finalization failed.");
    std::ostringstream result;
    for (unsigned i = 0; i < length; ++i)
        result << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(bytes[i]);
    return result.str();
}

std::string metadata(const fs::path &directory, Revision revision) {
    return "SMART_HOME_RUNTIME 1\n" + std::to_string(revision) + "\n" +
           digest(directory / "state.sqlite") + "\n" +
           digest(directory / "ontology.lbdb") + "\n";
}

struct Committed {
    fs::path path;
    Revision revision = 0;
};
Committed committed(const fs::path &state) {
    if (!fs::exists(fs::symlink_status(state / "CURRENT")))
        return {};
    auto current = readFile(state / "CURRENT");
    if (current.empty() || current.back() != '\n')
        throw std::runtime_error("Invalid CURRENT.");
    current.pop_back();
    if (current.rfind("gen-", 0) != 0 || current.size() > 100 ||
        current.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR"
                                  "STUVWXYZ0123456789-") != std::string::npos)
        throw std::runtime_error("Invalid CURRENT generation name.");
    const fs::path path = state / "generations" / current;
    if (!fs::is_directory(fs::symlink_status(path)))
        throw std::runtime_error("Missing committed generation.");
    const auto text = readFile(path / "metadata");
    std::istringstream input(text);
    std::string magic, version, revision;
    if (!(input >> magic >> version >> revision) ||
        magic != "SMART_HOME_RUNTIME" || version != "1")
        throw std::runtime_error("Unsupported runtime metadata.");
    const Revision rev = integer<Revision>(revision);
    if (rev < 0 || text != metadata(path, rev))
        throw std::runtime_error("Corrupt or mixed runtime generation.");
    return {path, rev};
}

class Database {
    struct Close {
        void operator()(sqlite3 *db) const { sqlite3_close_v2(db); }
    };
    std::unique_ptr<sqlite3, Close> db_;

  public:
    explicit Database(const fs::path &path) {
        sqlite3 *raw = nullptr;
        const int rc = sqlite3_open_v2(
            path.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            nullptr);
        db_.reset(raw);
        if (rc != SQLITE_OK)
            throw std::runtime_error("Cannot open runtime SQLite database.");
    }
    sqlite3 *get() const { return db_.get(); }
    void exec(const char *sql) {
        if (sqlite3_exec(get(), sql, nullptr, nullptr, nullptr) != SQLITE_OK)
            throw std::runtime_error("Runtime SQLite operation failed.");
    }
    using Statement =
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
    Statement prepare(const char *sql) {
        sqlite3_stmt *raw = nullptr;
        const int rc = sqlite3_prepare_v2(get(), sql, -1, &raw, nullptr);
        Statement statement(raw, sqlite3_finalize);
        if (rc != SQLITE_OK)
            throw std::runtime_error("Invalid runtime SQLite schema.");
        return statement;
    }
    void backup(const fs::path &path) {
        Database destination(path);
        sqlite3_backup *backup =
            sqlite3_backup_init(destination.get(), "main", get(), "main");
        if (!backup)
            throw std::runtime_error("SQLite backup initialization failed.");
        const int step = sqlite3_backup_step(backup, -1);
        const int finish = sqlite3_backup_finish(backup);
        if (step != SQLITE_DONE || finish != SQLITE_OK)
            throw std::runtime_error("SQLite backup failed.");
    }
};

struct Values {
    bool light = false;
    bool on = false;
    int brightness = 0;
    Color color{0, 0, 0};
    double temperature = 0;
    bool operator==(const Values &other) const {
        return light == other.light && on == other.on &&
               brightness == other.brightness && color == other.color &&
               temperature == other.temperature;
    }
};
using States = std::map<DeviceId, Values>;

std::string type(const DeviceRecord &record) {
    if (record.driver_id == "simulated.light")
        return "light";
    if (record.driver_id == "simulated.temperature_sensor")
        return "temperature_sensor";
    throw std::runtime_error("Unsupported saved driver type.");
}

States loadValues(Database &db, const std::vector<DeviceRecord> &records,
                  Revision revision, bool existing) {
    if (!existing) {
        db.exec("CREATE TABLE runtime_meta(version INTEGER NOT NULL, revision "
                "INTEGER NOT NULL);"
                "INSERT INTO runtime_meta VALUES(1,0);"
                "CREATE TABLE runtime_values(id INTEGER PRIMARY KEY, type TEXT "
                "NOT NULL,"
                "on_value INTEGER, brightness INTEGER, red INTEGER, green "
                "INTEGER, blue INTEGER, temperature REAL);");
        return {};
    }
    auto check = db.prepare("PRAGMA integrity_check");
    if (sqlite3_step(check.get()) != SQLITE_ROW ||
        std::string(reinterpret_cast<const char *>(
            sqlite3_column_text(check.get(), 0))) != "ok")
        throw std::runtime_error("Corrupt SQLite inventory.");
    auto meta = db.prepare("SELECT version,revision FROM runtime_meta");
    if (sqlite3_step(meta.get()) != SQLITE_ROW ||
        sqlite3_column_type(meta.get(), 0) != SQLITE_INTEGER ||
        sqlite3_column_int(meta.get(), 0) != 1 ||
        sqlite3_column_type(meta.get(), 1) != SQLITE_INTEGER ||
        sqlite3_column_int64(meta.get(), 1) != revision ||
        sqlite3_step(meta.get()) != SQLITE_DONE)
        throw std::runtime_error("Mismatched runtime SQLite revision/version.");
    States values;
    auto statement = db.prepare(
        "SELECT id,type,on_value,brightness,red,green,blue,temperature FROM "
        "runtime_values ORDER BY id");
    int rc;
    while ((rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
        auto *row = statement.get();
        if (sqlite3_column_type(row, 0) != SQLITE_INTEGER ||
            sqlite3_column_int64(row, 0) < 0 ||
            sqlite3_column_type(row, 1) != SQLITE_TEXT)
            throw std::runtime_error("Invalid saved device identity.");
        const auto id = sqlite3_column_int64(row, 0);
        const std::string saved_type(
            reinterpret_cast<const char *>(sqlite3_column_text(row, 1)));
        Values value;
        if (saved_type == "light") {
            value.light = true;
            for (int column = 2; column <= 6; ++column) {
                const auto number = sqlite3_column_int64(row, column);
                const int maximum = column == 2 ? 1 : column == 3 ? 100 : 255;
                if (sqlite3_column_type(row, column) != SQLITE_INTEGER ||
                    number < 0 || number > maximum)
                    throw std::runtime_error("Invalid saved light value.");
            }
            if (sqlite3_column_type(row, 7) != SQLITE_NULL)
                throw std::runtime_error("Unexpected light temperature.");
            value.on = sqlite3_column_int(row, 2) != 0;
            value.brightness = sqlite3_column_int(row, 3);
            value.color =
                Color{static_cast<std::uint8_t>(sqlite3_column_int(row, 4)),
                      static_cast<std::uint8_t>(sqlite3_column_int(row, 5)),
                      static_cast<std::uint8_t>(sqlite3_column_int(row, 6))};
        } else if (saved_type == "temperature_sensor") {
            for (int column = 2; column <= 6; ++column)
                if (sqlite3_column_type(row, column) != SQLITE_NULL)
                    throw std::runtime_error("Unexpected sensor light value.");
            const int storage_type = sqlite3_column_type(row, 7);
            value.temperature = sqlite3_column_double(row, 7);
            if ((storage_type != SQLITE_FLOAT &&
                 storage_type != SQLITE_INTEGER) ||
                !std::isfinite(value.temperature))
                throw std::runtime_error("Invalid saved temperature.");
        } else
            throw std::runtime_error("Unsupported saved device type.");
        if (!values.emplace(id, value).second)
            throw std::runtime_error("Duplicate saved device values.");
    }
    if (rc != SQLITE_DONE || values.size() != records.size())
        throw std::runtime_error("Incomplete saved device values.");
    for (const auto &record : records) {
        auto found = values.find(record.local_id);
        if (found == values.end() ||
            found->second.light != (type(record) == "light"))
            throw std::runtime_error("Inventory and values disagree.");
    }
    return values;
}

class TemperatureFactory final : public IDeviceDriverFactory {
    const States &restored_;

  public:
    explicit TemperatureFactory(const States &restored) : restored_(restored) {}
    std::string getDriverId() const override {
        return "simulated.temperature_sensor";
    }
    std::unique_ptr<IDeviceDriver>
    create(const DeviceRecord &record) const override {
        const auto found = restored_.find(record.local_id);
        return std::make_unique<SimulatedTemperatureSensorDriver>(
            record.local_id, record.physical_id, record.name,
            found == restored_.end() ? 23.0 : found->second.temperature);
    }
};

States capture(SQLiteDeviceStorage &storage, CommandDispatcher &dispatcher) {
    States result;
    for (const auto &record : storage.getAll()) {
        const auto get = [&](const char *operation) {
            return dispatcher.execute(Command(record.local_id, operation, {}))
                .value();
        };
        Values value;
        value.light = type(record) == "light";
        if (value.light) {
            value.on = std::get<bool>(get("on_off.get"));
            value.brightness = std::get<int>(get("brightness.get"));
            value.color = std::get<Color>(get("color.get"));
        } else
            value.temperature = std::get<double>(get("temperature.get"));
        result.emplace(record.local_id, value);
    }
    return result;
}

void storeValues(Database &db, const States &values, Revision revision) {
    db.exec("BEGIN IMMEDIATE");
    try {
        db.exec("DELETE FROM runtime_values");
        auto statement =
            db.prepare("INSERT INTO runtime_values VALUES(?,?,?,?,?,?,?,?)");
        for (const auto &[id, value] : values) {
            sqlite3_reset(statement.get());
            sqlite3_clear_bindings(statement.get());
            auto bind = [](int rc) {
                if (rc != SQLITE_OK)
                    throw std::runtime_error("SQLite binding failed.");
            };
            bind(sqlite3_bind_int64(statement.get(), 1, id));
            bind(sqlite3_bind_text(statement.get(), 2,
                                   value.light ? "light" : "temperature_sensor",
                                   -1, SQLITE_STATIC));
            if (value.light) {
                bind(sqlite3_bind_int(statement.get(), 3, value.on));
                bind(sqlite3_bind_int(statement.get(), 4, value.brightness));
                bind(sqlite3_bind_int(statement.get(), 5, value.color.red));
                bind(sqlite3_bind_int(statement.get(), 6, value.color.green));
                bind(sqlite3_bind_int(statement.get(), 7, value.color.blue));
            } else
                bind(
                    sqlite3_bind_double(statement.get(), 8, value.temperature));
            if (sqlite3_step(statement.get()) != SQLITE_DONE)
                throw std::runtime_error("Cannot store runtime values.");
        }
        auto meta = db.prepare("UPDATE runtime_meta SET revision=?");
        if (sqlite3_bind_int64(meta.get(), 1, revision) != SQLITE_OK ||
            sqlite3_step(meta.get()) != SQLITE_DONE)
            throw std::runtime_error("Cannot store runtime revision.");
        db.exec("COMMIT");
    } catch (...) {
        db.exec("ROLLBACK");
        throw;
    }
}

bool prepareGraph(Ontology &graph) {
    bool changed = false;
    for (const auto &room :
         {ontology::RoomRecord{"living_room", "Гостиная"},
          ontology::RoomRecord{"bathroom", "Ванная комната"}})
        if (!graph.getRoom(room.id)) {
            graph.addRoom(room);
            changed = true;
        }
    for (const auto &ability :
         {ontology::AbilityRecord{"on_off", "Включение", "lighting"},
          ontology::AbilityRecord{"brightness", "Яркость", "lighting"},
          ontology::AbilityRecord{"color", "Цвет", "lighting"},
          ontology::AbilityRecord{"temperature", "Температура", "climate"}})
        if (!graph.getAbility(ability.id)) {
            graph.addAbility(ability);
            changed = true;
        }
    return changed;
}

bool assignDefaultRooms(Ontology &graph, SQLiteDeviceStorage &storage) {
    bool changed = false;
    for (const auto &record : storage.getAll())
        if (!graph.getDeviceRoom(record.local_id)) {
            graph.assignDeviceToRoom(record.local_id, type(record) == "light"
                                                          ? "living_room"
                                                          : "bathroom");
            changed = true;
        }
    return changed;
}

using GraphSnapshot = std::map<DeviceId, ontology::DeviceOntologyDetails>;

GraphSnapshot snapshotGraph(Ontology &graph) {
    GraphSnapshot snapshot;
    for (const auto &device : graph.getAllDevices())
        snapshot.emplace(device.id, graph.getDeviceDetails(device.id).value());
    return snapshot;
}

void validateGraph(Ontology &graph, SQLiteDeviceStorage &storage,
                   DeviceRegistry &registry, const GraphSnapshot &snapshot) {
    const auto records = storage.getAll();
    if (snapshot.size() != records.size())
        throw std::runtime_error("Graph and inventory device counts disagree.");
    for (const auto &record : records) {
        const auto found = snapshot.find(record.local_id);
        if (found == snapshot.end())
            throw std::runtime_error("Graph and inventory disagree.");
        const auto &details = found->second;
        if (details.device.name != record.name || details.room_id.empty() ||
            !graph.getRoom(details.room_id))
            throw std::runtime_error("Graph and inventory disagree.");
        std::vector<std::string> expected;
        auto *driver = registry.find(record.local_id);
        if (!driver)
            throw std::runtime_error("Inventory driver missing.");
        for (const auto &capability : driver->descriptor().getCapabilities())
            expected.push_back(capability.getId());
        std::sort(expected.begin(), expected.end());
        if (details.ability_ids != expected)
            throw std::runtime_error(
                "Graph capabilities disagree with driver.");
    }
}

// Only the test executable contains fault injection; production ignores this
// environment.
void testPoint(const char *point, Revision revision) {
#ifdef SMART_HOME_RUNTIME_TESTING
    const char *base = std::getenv("SMART_HOME_RUNTIME_TEST_CONTROL");
    if (!base)
        return;
    const fs::path control(base);
    const std::string key = std::string(point) + "-" + std::to_string(revision);
    writeFile(control / (key + ".reached"), "ready\n");
    // A FIFO gives tests an explicit worker gate without timing assumptions.
    const fs::path gate = control / (key + ".gate");
    if (fs::exists(gate)) {
        Fd fd(::open(gate.c_str(), O_RDONLY | O_CLOEXEC));
        if (fd.get() < 0)
            systemError("test gate");
        char byte;
        while (::read(fd.get(), &byte, 1) < 0 && errno == EINTR) {
        }
    }
    if (fs::exists(control / (key + ".fail")))
        throw std::runtime_error("Injected runtime persistence failure.");
#else
    (void)point;
    (void)revision;
#endif
}

struct Snapshot {
    std::unique_ptr<Directory> directory;
    Revision revision;
};
struct SaveResult {
    Revision revision;
    bool success;
};

void publish(const fs::path &state, const Snapshot &snapshot) {
    const auto &source = snapshot.directory->path;
    const Revision revision = snapshot.revision;
    Directory temporary(state / "generations", ".pending-");
    testPoint("before-files", revision);
    for (const char *name : {"state.sqlite", "ontology.lbdb", "metadata"}) {
        fs::copy_file(source / name, temporary.path / name);
        syncPath(temporary.path / name);
        testPoint(name, revision);
    }
    syncPath(temporary.path, true);
    const auto name = "gen-" + std::to_string(revision) + "-" +
                      temporary.path.filename().string().substr(9);
    const auto generation = state / "generations" / name;
    fs::rename(temporary.path, generation);
    syncPath(state / "generations", true);
    testPoint("generation", revision);
    // Keep both old and new generations until CURRENT is durable.
    const auto current_tmp = state / "CURRENT.tmp";
    writeFile(current_tmp, name + "\n");
    syncPath(current_tmp);
    testPoint("before-current", revision);
    fs::rename(current_tmp, state / "CURRENT");
    testPoint("after-current", revision);
    syncPath(state, true);
    testPoint("committed", revision);
    // A failure in optional garbage collection must not revoke a durable
    // commit.
    try {
        std::error_code ignored;
        for (const auto &entry :
             fs::directory_iterator(state / "generations", ignored))
            if (entry.path() != generation)
                fs::remove_all(entry.path(), ignored);
    } catch (...) { /* Optional cleanup cannot revoke a durable commit. */
    }
}

class SaveWorker {
    fs::path state_;
    std::size_t capacity_, occupied_ = 0;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Snapshot> queue_;
    std::vector<SaveResult> results_;
    bool stopping_ = false;
    EventPipe events_;
    std::thread thread_;
    void work() noexcept {
        for (;;) {
            Snapshot snapshot;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
                if (queue_.empty())
                    return;
                snapshot = std::move(queue_.front());
                queue_.pop_front();
            }
            bool success = false;
            try {
                publish(state_, snapshot);
                success = true;
            } catch (...) {
                // Live RAM databases remain available for retry; failed queued
                // copies are bounded.
            }
            snapshot.directory.reset();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                results_.push_back({snapshot.revision, success});
            }
            events_.notify();
        }
    }

  public:
    SaveWorker(const fs::path &state, std::size_t capacity)
        : state_(state), capacity_(capacity), thread_() {
        results_.reserve(capacity_);
        thread_ = std::thread([this] { work(); });
    }
    ~SaveWorker() { stop(); }
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_one();
        if (thread_.joinable())
            thread_.join();
    }
    bool reserve() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (occupied_ == capacity_)
            return false;
        ++occupied_;
        return true;
    }
    void release() {
        std::lock_guard<std::mutex> lock(mutex_);
        --occupied_;
    }
    void submit(Snapshot snapshot) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(snapshot));
        }
        cv_.notify_one();
    }
    std::vector<SaveResult> collect() {
        events_.drain();
        std::lock_guard<std::mutex> lock(mutex_);
        auto results = results_;
        results_.clear();
        occupied_ -= results.size();
        return results;
    }
    int eventFd() const { return events_.reader(); }
};

class Reservation {
    SaveWorker &worker_;
    bool owned_;

  public:
    explicit Reservation(SaveWorker &worker)
        : worker_(worker), owned_(worker.reserve()) {}
    ~Reservation() {
        if (owned_)
            worker_.release();
    }
    explicit operator bool() const { return owned_; }
    void submit(Snapshot snapshot) {
        worker_.submit(std::move(snapshot));
        owned_ = false;
    }
};

std::string display(const Value &value) {
    return std::visit(
        [](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            std::ostringstream output;
            if constexpr (std::is_same_v<T, Color>)
                output << static_cast<int>(item.red) << ' '
                       << static_cast<int>(item.green) << ' '
                       << static_cast<int>(item.blue);
            else if constexpr (std::is_same_v<T, bool>)
                output << (item ? "true" : "false");
            else
                output << item;
            return output.str();
        },
        value);
}

std::vector<Value> commandArguments(const std::string &operation,
                                    const std::vector<std::string> &words,
                                    std::size_t start) {
    std::vector<Value> result;
    if (operation == "on_off.set" && words.size() == start + 1) {
        if (words[start] != "true" && words[start] != "false")
            throw std::invalid_argument("Boolean must be true or false.");
        result.emplace_back(words[start] == "true");
    } else if (operation == "brightness.set" && words.size() == start + 1)
        result.emplace_back(integer<int>(words[start]));
    else if (operation == "color.set" && words.size() == start + 3) {
        std::array<std::uint8_t, 3> rgb{};
        for (std::size_t i = 0; i < 3; ++i) {
            const int component = integer<int>(words[start + i]);
            if (component < 0 || component > 255)
                throw std::invalid_argument("RGB component must be 0..255.");
            rgb[i] = static_cast<std::uint8_t>(component);
        }
        result.emplace_back(Color{rgb[0], rgb[1], rgb[2]});
    } else if ((operation == "on_off.get" || operation == "brightness.get" ||
                operation == "color.get" || operation == "temperature.get") &&
               words.size() == start) {
    } else
        throw std::invalid_argument(
            "Unknown operation or wrong argument count.");
    return result;
}

int run(const Config &config) {
    EventPipe signal_events;
    Signals signals(signal_events);
    StateLock state_lock(config.state);
    const Committed saved = committed(config.state);
    requireRam(config.ram);
    Directory workspace(config.ram, "smart-home-runtime-");
    const auto inventory_path = workspace.path / "state.sqlite";
    const auto graph_path = workspace.path / "ontology.lbdb";
    if (!saved.path.empty()) {
        fs::copy_file(saved.path / "state.sqlite", inventory_path);
        fs::copy_file(saved.path / "ontology.lbdb", graph_path);
    }
    SQLiteDeviceStorage storage(inventory_path.string());
    Database database(inventory_path);
    const States restored = loadValues(database, storage.getAll(),
                                       saved.revision, !saved.path.empty());
    DriverFactoryRegistry factories;
    factories.add(std::make_unique<SimulatedLightDriverFactory>());
    factories.add(std::make_unique<TemperatureFactory>(restored));
    ontology::OntologyConfig graph_config;
    graph_config.database_path = graph_path;
    graph_config.ram_workspace_base = workspace.path;
    graph_config.allow_disk_fallback = false;
    Ontology graph(graph_config);
    // Preserve committed graph contents for validation before accepting startup.
    GraphSnapshot saved_graph;
    if (!saved.path.empty())
        saved_graph = snapshotGraph(graph);
    bool initialized = prepareGraph(graph);
    DeviceRegistry registry;
    DeviceManager manager(storage, factories, registry, graph);
    manager.restoreStoredDevices();
    CommandDispatcher dispatcher(registry);
    for (const auto &[id, values] : restored)
        if (values.light) {
            dispatcher.execute(Command(id, "on_off.set", {values.on}));
            dispatcher.execute(
                Command(id, "brightness.set", {values.brightness}));
            dispatcher.execute(Command(id, "color.set", {values.color}));
        }
    if (!saved.path.empty())
        validateGraph(graph, storage, registry, saved_graph);
    initialized = assignDefaultRooms(graph, storage) || initialized;
    std::vector<std::unique_ptr<IDeviceDiscoverySource>> sources;
    sources.push_back(std::make_unique<SimulatedDiscoverySource>(
        std::vector<DiscoveredDevice>{
            {"SIM-LIGHT-001", "simulated.light", "Light1"},
            {"SIM-LIGHT-002", "simulated.light", "Light2"},
            {"SIM-TEMP-001", "simulated.temperature_sensor",
             "TemperatureSensor in bath"}},
        false));
    DiscoveryService discovery(manager, std::move(sources));
    StdinFlags stdin_flags;
    SaveWorker worker(config.state, config.capacity);
    // All startup resources are ready. Keep live RAM state if runtime
    // completion fails.
    workspace.preserve = true;
    Revision revision = saved.revision, confirmed = saved.revision;
    bool unsaved = initialized;
    bool last_save_success = true;
    const auto advance = [&] {
        if (revision == std::numeric_limits<Revision>::max())
            throw std::runtime_error("Runtime revision overflow.");
        ++revision;
        unsaved = true;
    };
    if (initialized)
        advance();
    const auto collect = [&] {
        for (const auto &result : worker.collect()) {
            last_save_success = result.success;
            if (result.success) {
                confirmed = std::max(confirmed, result.revision);
                if (confirmed == revision)
                    unsaved = false;
                std::cout << "Saved revision " << result.revision << '\n';
            } else {
                std::cout << "Save failed for revision " << result.revision
                          << "; changes remain in RAM. Use save to retry.\n";
                log::log(log::Level::Error, "Runtime",
                         "Generation commit failed; RAM state retained.");
            }
        }
    };
    const auto prepare = [&](Reservation &reservation) {
        try {
            validateGraph(graph, storage, registry, snapshotGraph(graph));
            testPoint("prepare", revision);
            auto directory =
                std::make_unique<Directory>(workspace.path, "snapshot-");
            storeValues(database, capture(storage, dispatcher), revision);
            database.backup(directory->path / "state.sqlite");
            graph.dumpToFile((directory->path / "ontology.lbdb").string());
            writeFile(directory->path / "metadata",
                      metadata(directory->path, revision));
            reservation.submit({std::move(directory), revision});
            std::cout << "Save queued revision " << revision << '\n';
            return true;
        } catch (...) {
            unsaved = true;
            std::cout << "Save preparation failed; changes remain in RAM. Use "
                         "save to retry.\n";
            log::log(log::Level::Error, "Runtime",
                     "Snapshot preparation failed; RAM state retained.");
            return false;
        }
    };
    const auto scan = [&] {
        Reservation reservation(worker);
        if (!reservation) {
            std::cout << "Error: snapshot queue full; scan rejected before "
                         "execution.\n";
            return;
        }
        bool changed = false;
        const auto count = storage.getAll().size();
        try {
            const auto report = discovery.scanOnce();
            std::cout << "Scan: " << report.devices_processed << " processed, "
                      << report.errors.size() << " errors\n";
            changed = storage.getAll().size() != count;
            changed = assignDefaultRooms(graph, storage) || changed;
        } catch (...) {
            changed = true; // A partial inventory/graph operation must never be
                            // reported as a clean scan.
            std::cout << "Error: scan incomplete; changes remain in RAM. Retry "
                         "scan.\n";
            log::log(log::Level::Error, "Runtime",
                     "Discovery/graph operation incomplete.");
        }
        if (changed)
            advance();
        if (changed || initialized) {
            initialized = false;
            prepare(reservation);
        }
    };
    // Console categories are a runtime convention; ontology stores no type field.
    const auto consoleType = [&](DeviceId id) {
        for (const auto &record : storage.getAll())
            if (record.local_id == id)
                return type(record);
        throw std::runtime_error("Graph device is absent from inventory.");
    };
    const auto show = [&](DeviceId id) {
        const auto details = graph.getDeviceDetails(id);
        if (!details)
            throw std::invalid_argument("Unknown device ID.");
        std::cout << "Device " << id << " " << details->device.name << " "
                  << consoleType(id) << " room=" << details->room_id
                  << '\n';
        for (const auto &ability : details->ability_ids) {
            const auto value =
                dispatcher.execute(Command(id, ability + ".get", {}));
            std::cout << "  " << ability << " = " << display(value.value())
                      << '\n';
        }
    };
    bool exiting = false;
    const auto command = [&](const std::string &line) {
        std::istringstream input(line);
        std::vector<std::string> words;
        for (std::string word; input >> word;)
            words.push_back(std::move(word));
        if (words.empty())
            return;
        try {
            if (words[0] == "help" && words.size() == 1) {
                std::cout << "help | list | show <id> | scan | save | exit\n"
                             "<id> <operation> [arguments]\n<room> <type> "
                             "<operation> [arguments]\n"
                             "Rooms: living_room, bathroom; types: light, "
                             "temperature_sensor\n"
                             "Operations: on_off.get/set (true|false), "
                             "brightness.get/set (0..100), color.get/set (R G "
                             "B, 0..255), temperature.get\n";
                return;
            }
            if (words[0] == "exit" && words.size() == 1) {
                exiting = true;
                return;
            }
            if (words[0] == "list" && words.size() == 1) {
                for (const auto &device : graph.getAllDevices()) {
                    const auto room = graph.getDeviceRoom(device.id);
                    std::cout << "Device " << device.id << " " << device.name
                              << " " << consoleType(device.id)
                              << " room=" << (room ? room->id : "") << '\n';
                }
                return;
            }
            if (words[0] == "show" && words.size() == 2) {
                const auto id = integer<DeviceId>(words[1]);
                if (id < 0)
                    throw std::invalid_argument(
                        "Device ID must be nonnegative.");
                show(id);
                return;
            }
            if (words[0] == "scan" && words.size() == 1) {
                scan();
                return;
            }
            if (words[0] == "save" && words.size() == 1) {
                Reservation reservation(worker);
                if (!reservation)
                    throw std::runtime_error(
                        "Snapshot queue full; save was not queued.");
                prepare(reservation);
                return;
            }
            std::vector<DeviceId> targets;
            std::size_t operation_index;
            const bool address = !words[0].empty() &&
                                 (words[0][0] == '-' ||
                                  (words[0][0] >= '0' && words[0][0] <= '9'));
            if (address) {
                const auto id = integer<DeviceId>(words[0]);
                if (id < 0)
                    throw std::invalid_argument(
                        "Device ID must be nonnegative.");
                targets.push_back(id);
                operation_index = 1;
            } else {
                if (words.size() < 3)
                    throw std::invalid_argument(
                        "Expected room, type and operation.");
                if (words[1] != "light" && words[1] != "temperature_sensor")
                    throw std::invalid_argument("Unknown device type.");
                for (const auto &device : graph.getDevicesInRoom(words[0]))
                    if (consoleType(device.id) == words[1])
                        targets.push_back(device.id);
                if (targets.empty())
                    throw std::invalid_argument("No matching devices.");
                operation_index = 2;
            }
            if (words.size() <= operation_index)
                throw std::invalid_argument("Missing operation.");
            const auto &operation = words[operation_index];
            const auto values =
                commandArguments(operation, words, operation_index + 1);
            const bool mutating =
                operation.size() >= 4 &&
                operation.substr(operation.size() - 4) == ".set";
            // Even a no-op setter reserves before execution, then releases the
            // unused slot.
            std::unique_ptr<Reservation> reservation;
            States before;
            if (mutating) {
                reservation = std::make_unique<Reservation>(worker);
                if (!*reservation)
                    throw std::runtime_error("Snapshot queue full; change "
                                             "rejected before execution.");
                before = capture(storage, dispatcher);
            }
            std::sort(targets.begin(), targets.end());
            for (const auto id : targets) {
                try {
                    testPoint(("target-" + std::to_string(id)).c_str(),
                              revision);
                    const auto result =
                        dispatcher.execute(Command(id, operation, values));
                    std::cout << "Device " << id << " " << operation << ": "
                              << (result ? display(*result) : "applied")
                              << '\n';
                } catch (const std::exception &error) {
                    std::cout << "Device " << id << " error: " << error.what()
                              << '\n';
                    log::log(log::Level::Warn, "Runtime",
                             "Device command failed.");
                }
            }
            if (mutating && !(before == capture(storage, dispatcher))) {
                advance();
                std::cout << "Changes applied; save pending revision "
                          << revision << '\n';
                prepare(*reservation);
            }
        } catch (const std::exception &error) {
            std::cout << "Error: " << error.what() << '\n';
            log::log(log::Level::Warn, "Runtime", "Console command rejected.");
        }
    };
    std::cout << "Runtime ready. Type help.\n" << std::flush;
    bool first_iteration = true;
    std::string pending;
    bool eof = false, discard = false;
    while (!exiting && !stop_requested) {
        collect();
        if (first_iteration) {
            first_iteration = false;
            scan();
            continue;
        }
        const auto newline = pending.find('\n');
        if (newline != std::string::npos) {
            const auto line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (line.size() > 65536)
                std::cout << "Error: input line too long.\n";
            else
                command(line);
            std::cout.flush();
            continue;
        }
        if (eof) {
            if (!pending.empty())
                command(pending);
            break;
        }
        std::cout.flush();
        pollfd fds[] = {{STDIN_FILENO, POLLIN, 0},
                        {worker.eventFd(), POLLIN, 0},
                        {signal_events.reader(), POLLIN, 0}};
        const int result = ::poll(fds, 3, -1);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            systemError("poll");
        }
        if (fds[2].revents)
            signal_events.drain();
        if (fds[0].revents & POLLNVAL)
            throw std::runtime_error("stdin unavailable.");
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char bytes[4096];
            const auto size = ::read(STDIN_FILENO, bytes, sizeof(bytes));
            if (size == 0)
                eof = true;
            else if (size < 0) {
                if (errno != EAGAIN && errno != EINTR)
                    systemError("read stdin");
            } else
                for (ssize_t i = 0; i < size; ++i) {
                    if (discard) {
                        if (bytes[i] == '\n')
                            discard = false;
                        continue;
                    }
                    pending.push_back(bytes[i]);
                    if (pending.size() > 65536 &&
                        pending.find('\n') == std::string::npos) {
                        pending.clear();
                        discard = true;
                        std::cout << "Error: input line too long.\n";
                    }
                }
        }
    }
    std::cout << "Stopping; final save pending.\n" << std::flush;
    bool final_queued = false;
    for (;;) {
        collect();
        if (!final_queued) {
            Reservation reservation(worker);
            if (reservation) {
                final_queued = prepare(reservation);
                break;
            }
        }
        pollfd event{worker.eventFd(), POLLIN, 0};
        if (::poll(&event, 1, -1) < 0 && errno != EINTR)
            systemError("poll final save");
    }
    worker.stop();
    collect();
    bool closed = true;
    try {
        graph.close();
    } catch (...) {
        closed = false;
        log::log(log::Level::Error, "Runtime", "Final ontology close failed.");
    }
    const bool success = final_queued && last_save_success &&
                         confirmed == revision && !unsaved && closed;
    workspace.preserve = !success;
    if (!success)
        std::cerr << "Final save failed; recoverable RAM files retained at "
                  << workspace.path << '\n';
    return success ? 0 : 1;
}
} // namespace

int main(int argc, char *argv[]) {
    Config config;
    log::Config logging_config;
    std::vector<const char *> logging_arguments{argv[0]};
    try {
        config = arguments(argc, argv, logging_arguments);
    } catch (const std::exception &error) {
        std::cerr << "Runtime configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        logging_config =
            log::resolveConfig(static_cast<int>(logging_arguments.size()),
                               logging_arguments.data(), std::cerr);
    } catch (const std::invalid_argument &error) {
        std::cerr << "Logging configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        log::Session session(logging_config);
        log::log(log::Level::Info, "Main", "Application started.");
        try {
            const int result = run(config);
            log::log(log::Level::Info, "Main", "Application finished.");
            return result;
        } catch (const std::exception &error) {
            std::cerr << "Runtime failure: " << error.what() << '\n';
            log::log(log::Level::Critical, "Main",
                     "Unhandled application failure; exiting.");
            return 1;
        } catch (...) {
            log::log(log::Level::Critical, "Main",
                     "Unknown application failure; exiting.");
            return 1;
        }
    } catch (...) {
        std::cerr << "Cannot initialize logging configuration.\n";
        return 2;
    }
}
