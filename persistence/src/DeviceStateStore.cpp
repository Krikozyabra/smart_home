#include "persistence/DeviceStateStore.h"
#include "FileUtils.h"
#include "GenerationFiles.h"
#include "persistence/PersistenceConfig.h"

#include <sqlite3.h>

#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace fs = std::filesystem;

namespace smart_home::persistence {
namespace {

constexpr auto properties_schema =
    "CREATE TABLE IF NOT EXISTS device_properties ("
    "device_id INTEGER NOT NULL, property_key TEXT NOT NULL,"
    "value_type TEXT NOT NULL, raw_value TEXT NOT NULL, updated_at INTEGER NOT "
    "NULL,"
    "PRIMARY KEY(device_id, property_key),"
    "FOREIGN KEY(device_id) REFERENCES devices(local_id) ON DELETE CASCADE)";

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

class Database {
  public:
    Database(const fs::path &path, int flags) {
        sqlite3 *raw = nullptr;
        const int result = sqlite3_open_v2(path.c_str(), &raw, flags, nullptr);
        db_.reset(raw);
        check(result);
    }
    sqlite3 *get() const noexcept { return db_.get(); }
    void check(int result) const {
        if (result != SQLITE_OK)
            throw std::runtime_error(
                std::string("Runtime SQLite: ") +
                (db_ ? sqlite3_errmsg(db_.get()) : sqlite3_errstr(result)));
    }
    void exec(const char *sql) const {
        check(sqlite3_exec(get(), sql, nullptr, nullptr, nullptr));
    }
    Statement prepare(const char *sql) const {
        sqlite3_stmt *raw = nullptr;
        const int result = sqlite3_prepare_v2(get(), sql, -1, &raw, nullptr);
        Statement statement(raw, sqlite3_finalize);
        check(result);
        return statement;
    }
    void close() {
        check(sqlite3_close(get()));
        db_.release();
    }

  private:
    std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> db_{nullptr,
                                                              sqlite3_close_v2};
};

class Transaction {
  public:
    explicit Transaction(const Database &db) : db_(db) {
        db_.exec("BEGIN IMMEDIATE");
    }
    ~Transaction() {
        if (active_)
            sqlite3_exec(db_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
    }
    void commit() {
        db_.exec("COMMIT");
        active_ = false;
    }

  private:
    const Database &db_;
    bool active_ = true;
};

std::int64_t integerColumn(sqlite3_stmt *row, int column) {
    if (sqlite3_column_type(row, column) != SQLITE_INTEGER)
        throw std::runtime_error("Invalid integer in runtime SQLite.");
    return sqlite3_column_int64(row, column);
}

std::string textColumn(sqlite3_stmt *row, int column) {
    if (sqlite3_column_type(row, column) != SQLITE_TEXT)
        throw std::runtime_error("Invalid text in runtime SQLite.");
    const auto *text =
        reinterpret_cast<const char *>(sqlite3_column_text(row, column));
    if (!text)
        throw std::runtime_error("Cannot read runtime SQLite text.");
    return {text, static_cast<std::size_t>(sqlite3_column_bytes(row, column))};
}

void validateRevision(const Database &db, Revision expected) {
    const auto statement =
        db.prepare("SELECT version,revision FROM runtime_meta");
    auto *row = statement.get();
    if (expected < 0 || sqlite3_step(row) != SQLITE_ROW ||
        integerColumn(row, 0) != 1 || integerColumn(row, 1) != expected ||
        sqlite3_step(row) != SQLITE_DONE)
        throw std::runtime_error("Mismatched runtime SQLite revision/version.");
}

bool hasTable(const Database &db, const char *name) {
    const auto statement =
        db.prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
    db.check(sqlite3_bind_text(statement.get(), 1, name, -1, SQLITE_STATIC));
    const int result = sqlite3_step(statement.get());
    if (result == SQLITE_ROW)
        return true;
    if (result != SQLITE_DONE)
        db.check(result);
    return false;
}

std::int64_t parseInteger(std::string_view raw) {
    if (raw.empty())
        throw std::runtime_error("Invalid integer in device properties.");
    std::int64_t value{};
    const auto *end = raw.data() + raw.size();
    const auto result = std::from_chars(raw.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        throw std::runtime_error("Invalid integer in device properties.");
    return value;
}

std::uint8_t colorComponent(std::int64_t value) {
    if (value < 0 || value > 255)
        throw std::runtime_error("Invalid RGB component in device properties.");
    return static_cast<std::uint8_t>(value);
}

PropertyValue decode(const std::string &type, const std::string &raw) {
    if (type == "bool") {
        if (raw == "true")
            return true;
        if (raw == "false")
            return false;
    } else if (type == "int") {
        return parseInteger(raw);
    } else if (type == "double") {
        std::istringstream input(raw);
        input.imbue(std::locale::classic());
        double value{};
        if (input >> std::noskipws >> value && input.eof() &&
            std::isfinite(value))
            return value;
    } else if (type == "color") {
        const auto first = raw.find(',');
        const auto last = raw.rfind(',');
        if (first != std::string::npos && first != last) {
            const std::string_view text(raw);
            return Color{colorComponent(parseInteger(text.substr(0, first))),
                         colorComponent(parseInteger(
                             text.substr(first + 1, last - first - 1))),
                         colorComponent(parseInteger(text.substr(last + 1)))};
        }
    } else if (type == "string") {
        return raw;
    }
    throw std::runtime_error("Invalid type or value in device properties.");
}

struct Encoded {
    const char *type;
    std::string raw;
};

Encoded encode(const PropertyValue &property) {
    return std::visit(
        [](const auto &value) -> Encoded {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) {
                return {"bool", value ? "true" : "false"};
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return {"int", std::to_string(value)};
            } else if constexpr (std::is_same_v<T, double>) {
                if (!std::isfinite(value))
                    throw std::runtime_error("Non-finite property value.");
                std::ostringstream output;
                output.exceptions(std::ios::badbit | std::ios::failbit);
                output.imbue(std::locale::classic());
                output << std::setprecision(
                              std::numeric_limits<double>::max_digits10)
                       << value;
                return {"double", output.str()};
            } else if constexpr (std::is_same_v<T, Color>) {
                return {"color", std::to_string(value.red) + "," +
                                     std::to_string(value.green) + "," +
                                     std::to_string(value.blue)};
            } else {
                return {"string", value};
            }
        },
        property);
}

DevicePropertiesMap readProperties(const Database &db) {
    const auto statement = db.prepare("SELECT "
                                      "p.device_id,p.property_key,p.value_type,"
                                      "p.raw_value,p.updated_at,d.local_id "
                                      "FROM device_properties p LEFT JOIN "
                                      "devices d ON p.device_id=d.local_id");
    auto *row = statement.get();
    DevicePropertiesMap properties;
    int result;
    while ((result = sqlite3_step(row)) == SQLITE_ROW) {
        const auto id = integerColumn(row, 0);
        const auto key = textColumn(row, 1);
        if (id < 0 || key.empty() || integerColumn(row, 5) != id)
            throw std::runtime_error("Invalid device property identity.");
        integerColumn(row, 4);
        if (!properties[id]
                 .emplace(key, decode(textColumn(row, 2), textColumn(row, 3)))
                 .second)
            throw std::runtime_error("Duplicate device property.");
    }
    if (result != SQLITE_DONE)
        db.check(result);
    return properties;
}

DevicePropertiesMap readLegacyProperties(const Database &db) {
    // v1 initially stored a single row per simulated device.
    const auto statement = db.prepare(
        "SELECT "
        "v.id,v.type,v.on_value,v.brightness,v.red,v.green,v.blue,v."
        "temperature,d.local_id "
        "FROM runtime_values v LEFT JOIN devices d ON v.id=d.local_id");
    auto *row = statement.get();
    DevicePropertiesMap properties;
    int result;
    while ((result = sqlite3_step(row)) == SQLITE_ROW) {
        const auto id = integerColumn(row, 0);
        if (id < 0 || integerColumn(row, 8) != id)
            throw std::runtime_error("Invalid saved device identity.");
        auto [device, inserted] = properties.try_emplace(id);
        if (!inserted)
            throw std::runtime_error("Duplicate saved device values.");
        auto &values = device->second;
        const auto type = textColumn(row, 1);
        if (type == "light") {
            const auto on = integerColumn(row, 2);
            const auto brightness = integerColumn(row, 3);
            if (on < 0 || on > 1 || brightness < 0 || brightness > 100 ||
                sqlite3_column_type(row, 7) != SQLITE_NULL)
                throw std::runtime_error("Invalid saved light value.");
            values.emplace("on_off", on != 0);
            values.emplace("brightness", brightness);
            values.emplace("color",
                           Color{colorComponent(integerColumn(row, 4)),
                                 colorComponent(integerColumn(row, 5)),
                                 colorComponent(integerColumn(row, 6))});
        } else if (type == "temperature_sensor") {
            for (int column = 2; column <= 6; ++column)
                if (sqlite3_column_type(row, column) != SQLITE_NULL)
                    throw std::runtime_error("Unexpected sensor light value.");
            const int storage_type = sqlite3_column_type(row, 7);
            const double value = sqlite3_column_double(row, 7);
            if ((storage_type != SQLITE_FLOAT &&
                 storage_type != SQLITE_INTEGER) ||
                !std::isfinite(value))
                throw std::runtime_error("Invalid saved temperature.");
            values.emplace("temperature", value);
        } else {
            throw std::runtime_error("Unsupported saved device type.");
        }
    }
    if (result != SQLITE_DONE)
        db.check(result);
    return properties;
}

void bindText(const Database &db, sqlite3_stmt *statement, int column,
              const std::string &value) {
    db.check(sqlite3_bind_text64(statement, column, value.data(), value.size(),
                                 SQLITE_TRANSIENT, SQLITE_UTF8));
}

constexpr std::array sidecar_suffixes{"-journal", "-wal", "-shm"};

void requireNoSidecars(const fs::path &path) {
    for (const char *suffix : sidecar_suffixes)
        if (fs::exists(fs::symlink_status(path.string() + suffix)))
            throw std::runtime_error(
                "Runtime snapshot must be a standalone SQLite file.");
}

} // namespace

class DeviceStateStore::Impl {
  public:
    explicit Impl(const fs::path &path)
        : db(detail::absolutePath(path), SQLITE_OPEN_READWRITE) {
        db.exec("PRAGMA foreign_keys=ON");
    }
    Database db;
};

DeviceStateStore::DeviceStateStore(const fs::path &database_path)
    : impl_(std::make_unique<Impl>(database_path)) {}
DeviceStateStore::~DeviceStateStore() = default;

void DeviceStateStore::initializeSchema() {
    const auto &db = impl_->db;
    Transaction transaction(db);
    db.exec("CREATE TABLE runtime_meta(version INTEGER NOT NULL, revision "
            "INTEGER NOT NULL);"
            "INSERT INTO runtime_meta VALUES(1,0);");
    db.exec(properties_schema);
    transaction.commit();
}

DevicePropertiesMap
DeviceStateStore::loadProperties(Revision expected_revision) const {
    const auto &db = impl_->db;
    const auto check = db.prepare("PRAGMA integrity_check");
    if (sqlite3_step(check.get()) != SQLITE_ROW ||
        textColumn(check.get(), 0) != "ok" ||
        sqlite3_step(check.get()) != SQLITE_DONE)
        throw std::runtime_error("Corrupt SQLite inventory.");
    validateRevision(db, expected_revision);
    const bool current = hasTable(db, "device_properties");
    const bool legacy = hasTable(db, "runtime_values");
    if (current == legacy)
        throw std::runtime_error("Invalid runtime properties schema.");
    return current ? readProperties(db) : readLegacyProperties(db);
}

void DeviceStateStore::storeProperties(const DevicePropertiesMap &properties,
                                       Revision revision) {
    if (revision < 0)
        throw std::runtime_error("Runtime revision must be nonnegative.");
    const auto &db = impl_->db;
    Transaction transaction(db);
    db.exec(properties_schema);
    db.exec("DELETE FROM device_properties");
    const auto statement = db.prepare("INSERT INTO "
                                      "device_properties(device_id,property_"
                                      "key,value_type,raw_value,updated_at) "
                                      "VALUES(?,?,?,?,?)");
    auto *row = statement.get();
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    for (const auto &[id, values] : properties) {
        for (const auto &[key, property] : values) {
            if (id < 0 || key.empty())
                throw std::runtime_error("Invalid device property identity.");
            const auto value = encode(property);
            db.check(sqlite3_bind_int64(row, 1, id));
            bindText(db, row, 2, key);
            db.check(sqlite3_bind_text(row, 3, value.type, -1, SQLITE_STATIC));
            bindText(db, row, 4, value.raw);
            db.check(sqlite3_bind_int64(row, 5, timestamp));
            if (const int result = sqlite3_step(row); result != SQLITE_DONE)
                db.check(result);
            db.check(sqlite3_reset(row));
        }
    }
    const auto meta = db.prepare("UPDATE runtime_meta SET revision=?1 WHERE "
                                 "version=1 AND revision BETWEEN 0 AND ?1");
    db.check(sqlite3_bind_int64(meta.get(), 1, revision));
    if (const int result = sqlite3_step(meta.get()); result != SQLITE_DONE)
        db.check(result);
    if (sqlite3_changes(db.get()) != 1)
        throw std::runtime_error("Cannot store runtime revision.");
    db.exec("DROP TABLE IF EXISTS runtime_values");
    transaction.commit();
}

void DeviceStateStore::backupTo(const fs::path &target) const {
    const auto path = detail::absolutePath(target);
    requireRamDirectory(path.parent_path());
    requireNoSidecars(path);
    detail::FileDescriptor file(
        detail::openPath(path, O_WRONLY | O_CREAT | O_EXCL));
    try {
        file.close();
        Database destination(path, SQLITE_OPEN_READWRITE);
        sqlite3_backup *backup = sqlite3_backup_init(destination.get(), "main",
                                                     impl_->db.get(), "main");
        if (!backup)
            throw std::runtime_error("SQLite backup initialization failed.");
        const int copied = sqlite3_backup_step(backup, -1);
        const int finished = sqlite3_backup_finish(backup);
        if (copied != SQLITE_DONE)
            throw std::runtime_error(std::string("SQLite backup failed: ") +
                                     sqlite3_errstr(copied));
        destination.check(finished);
        {
            const auto mode = destination.prepare("PRAGMA journal_mode=DELETE");
            if (sqlite3_step(mode.get()) != SQLITE_ROW ||
                textColumn(mode.get(), 0) != "delete")
                throw std::runtime_error(
                    "Cannot create standalone runtime SQLite snapshot.");
        }
        destination.close();
    } catch (...) {
        detail::removeTree(path);
        try {
            for (const char *suffix : sidecar_suffixes)
                detail::removeTree(path.string() + suffix);
        } catch (...) {
            // Preserve the backup error if cleanup cannot build a sidecar path.
        }
        throw;
    }
}

namespace detail {

void validateSQLiteRevision(const std::filesystem::path &path,
                            Revision revision) {
    const FileDescriptor file(openRegularFile(path));
    requireNoSidecars(path);
    // An immutable URI avoids journal creation in the published generation.
    std::string uri = "file:";
    for (const char ch : path.native()) {
        if (ch == '%')
            uri += "%25";
        else if (ch == '?')
            uri += "%3F";
        else if (ch == '#')
            uri += "%23";
        else
            uri += ch;
    }
    const Database db(uri + "?immutable=1",
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_URI);
    validateRevision(db, revision);
    db.prepare("SELECT local_id FROM devices LIMIT 0");
}

} // namespace detail
} // namespace smart_home::persistence
