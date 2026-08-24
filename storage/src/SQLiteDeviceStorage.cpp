#include "storage/SQLiteDeviceStorage.h"
#include "sqlite3.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using StatementPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

std::string readText(sqlite3_stmt *statement, int column) {
    const auto *text = sqlite3_column_text(statement, column);

    if (text == nullptr) {
        throw std::runtime_error("Unexpected NULL in devices table");
    }

    return reinterpret_cast<const char *>(text);
}

void bindText(sqlite3 *database, sqlite3_stmt *statement, int index,
              const std::string &value) {
    const int rc = sqlite3_bind_text(statement, index, value.c_str(), -1,
                                     SQLITE_TRANSIENT);

    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to bind SQLite parameter: " +
                                 std::string(sqlite3_errmsg(database)));
    }
}

} // namespace

namespace smart_home {

void SQLiteDeviceStorage::createSchema() {
    std::string create_table_sql =
        "CREATE TABLE IF NOT EXISTS devices ("
        "local_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "physical_id TEXT NOT NULL CHECK (physical_id <> ''),"
        "driver_id   TEXT NOT NULL CHECK (driver_id <> ''),"
        "name        TEXT NOT NULL,"
        "UNIQUE (physical_id, driver_id)"
        ");";

    char *raw_error = nullptr;
    int rc = sqlite3_exec(database, create_table_sql.c_str(), nullptr, nullptr,
                          &raw_error);
    if (rc != SQLITE_OK) {
        std::string err_msg =
            (raw_error != nullptr ? raw_error : sqlite3_errmsg(database));

        sqlite3_free(raw_error);

        throw std::runtime_error(err_msg);
    }
}

SQLiteDeviceStorage::SQLiteDeviceStorage(const std::string &database_path) {
    int rc =
        sqlite3_open_v2(database_path.c_str(), &database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        const std::string msg = (database != nullptr ? sqlite3_errmsg(database)
                                                     : sqlite3_errstr(rc));

        if (database != nullptr) {
            sqlite3_close_v2(database);
            database = nullptr;
        }
        throw std::runtime_error("Failed to open SQLite database: " + msg);
    }
    try {
        createSchema();
    } catch (...) {
        sqlite3_close_v2(database);
        database = nullptr;
        throw;
    }
}

SQLiteDeviceStorage::~SQLiteDeviceStorage() {
    if (database != nullptr) {
        sqlite3_close_v2(database);
        database = nullptr;
    }
}

std::optional<DeviceRecord>
SQLiteDeviceStorage::findByPhysicalId(const std::string &physical_id,
                                      const std::string &driver_id) const {
    if (physical_id.empty())
        throw std::invalid_argument("Physical id must not be empty");
    if (driver_id.empty())
        throw std::invalid_argument("Driver id must not be empty");

    std::string find_sql = "SELECT local_id, physical_id, driver_id, name "
                           "FROM devices "
                           "WHERE physical_id = ? AND driver_id = ?;";
    StatementPtr find_stmt(nullptr, sqlite3_finalize);
    sqlite3_stmt *raw_stmt = nullptr;
    int rc =
        sqlite3_prepare_v2(database, find_sql.c_str(), -1, &raw_stmt, nullptr);

    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " +
                                 std::string(sqlite3_errmsg(database)));
    }

    find_stmt.reset(raw_stmt);

    bindText(database, find_stmt.get(), 1, physical_id);
    bindText(database, find_stmt.get(), 2, driver_id);

    rc = sqlite3_step(find_stmt.get());
    if (rc == SQLITE_ROW) {
        DeviceRecord record;
        record.local_id = sqlite3_column_int64(find_stmt.get(), 0);
        record.physical_id = readText(find_stmt.get(), 1);
        record.driver_id = readText(find_stmt.get(), 2);
        record.name = readText(find_stmt.get(), 3);

        return record;
    } else if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    throw std::runtime_error(std::string(sqlite3_errmsg(database)));
}

DeviceRecord SQLiteDeviceStorage::insert(const std::string &physical_id,
                                         const std::string &driver_id,
                                         const std::string &default_name) {
    if (physical_id.empty())
        throw std::invalid_argument("Physical id must not be empty");
    if (driver_id.empty())
        throw std::invalid_argument("Driver id must not be empty");

    std::string insert_sql =
        "INSERT INTO devices (physical_id, driver_id, name) "
        "VALUES (?, ?, ?);";
    StatementPtr insert_stmt(nullptr, sqlite3_finalize);
    sqlite3_stmt *raw_stmt = nullptr;
    int rc = sqlite3_prepare_v2(database, insert_sql.c_str(), -1, &raw_stmt,
                                nullptr);

    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " +
                                 std::string(sqlite3_errmsg(database)));
    }

    insert_stmt.reset(raw_stmt);

    bindText(database, insert_stmt.get(), 1, physical_id);
    bindText(database, insert_stmt.get(), 2, driver_id);
    bindText(database, insert_stmt.get(), 3, default_name);

    rc = sqlite3_step(insert_stmt.get());
    if (rc == SQLITE_DONE) {
        const sqlite3_int64 last_id = sqlite3_last_insert_rowid(database);
        if (last_id < 0)
            throw std::runtime_error(
                "Insert failed: inserted device have got the id lower than 0");
        return DeviceRecord{last_id, physical_id, driver_id, default_name};
    }

    if (sqlite3_extended_errcode(database) == SQLITE_CONSTRAINT_UNIQUE)
        throw std::invalid_argument(
            "Given pair physical id and driver id already in database");
    throw std::runtime_error("Insert failed: " +
                             std::string(sqlite3_errmsg(database)));
}

std::vector<DeviceRecord> SQLiteDeviceStorage::getAll() const {
    std::vector<DeviceRecord> result;
    std::string find_all_sql = "SELECT local_id, physical_id, driver_id, name "
                               "FROM devices;";
    StatementPtr find_all_stmt(nullptr, sqlite3_finalize);
    sqlite3_stmt *raw_stmt = nullptr;
    int rc = sqlite3_prepare_v2(database, find_all_sql.c_str(), -1, &raw_stmt,
                                nullptr);

    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " +
                                 std::string(sqlite3_errmsg(database)));
    }

    find_all_stmt.reset(raw_stmt);
    while ((rc = sqlite3_step(find_all_stmt.get())) == SQLITE_ROW) {
        DeviceRecord record;
        record.local_id = sqlite3_column_int64(find_all_stmt.get(), 0);
        record.physical_id = readText(find_all_stmt.get(), 1);
        record.driver_id = readText(find_all_stmt.get(), 2);
        record.name = readText(find_all_stmt.get(), 3);
        result.push_back(record);
    }
    if (rc == SQLITE_DONE)
        return result;
    throw std::runtime_error(std::string(sqlite3_errmsg(database)));
}
} // namespace smart_home
