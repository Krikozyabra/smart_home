#include "PersistenceTestSupport.h"

#include <sqlite3.h>

#include <iostream>
#include <limits>
#include <memory>

using namespace persistence_test;

namespace {

void enableWal(const fs::path &path) {
    sqlite3 *raw = nullptr;
    const auto rc =
        sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> db(
        raw, sqlite3_close_v2);
    require(rc == SQLITE_OK, "Open inventory for WAL configuration");
    require(sqlite3_exec(db.get(), "PRAGMA journal_mode=WAL", nullptr, nullptr,
                         nullptr) == SQLITE_OK,
            "Enable WAL on the working database");
}

void roundTripAndRollback() {
    Directory working;
    Directory ram("/dev/shm");
    const auto path = working.path / "state.sqlite";
    smart_home::SQLiteDeviceStorage inventory(path.string());
    const auto device = inventory.insert("test", "driver", "Лампа");
    enableWal(path);
    DeviceStateStore store(path);
    store.initializeSchema();
    require(store.loadProperties(0).empty(),
            "Fresh schema has no property rows");

    const DevicePropertiesMap original{
        {device.local_id,
         {{"on_off", true},
          {"brightness", std::numeric_limits<std::int64_t>::max()},
          {"offset", std::numeric_limits<std::int64_t>::min()},
          {"temperature", 23.123456789012345},
          {"small", std::numeric_limits<double>::denorm_min()},
          {"color", Color{0, 128, 255}},
          {"label", std::string("A\0B", 3)}}}};
    store.storeProperties(original, 1);
    require(store.loadProperties(1) == original,
            "All property types round-trip exactly");
    expectException<std::runtime_error>([&] { store.loadProperties(0); });

    auto invalid = original;
    invalid[device.local_id + 1]["brightness"] = std::int64_t{60};
    expectException<std::runtime_error>(
        [&] { store.storeProperties(invalid, 2); });
    require(store.loadProperties(1) == original,
            "Foreign key failure rolls back properties and revision");

    invalid = original;
    invalid[device.local_id]["temperature"] =
        std::numeric_limits<double>::infinity();
    expectException<std::runtime_error>(
        [&] { store.storeProperties(invalid, 2); });
    expectException<std::runtime_error>([&] { store.storeProperties({}, 0); });
    expectException<std::runtime_error>([&] { store.storeProperties({}, -1); });
    require(store.loadProperties(1) == original,
            "Rejected writes leave the database unchanged");

    const auto backup = ram.path / "backup.sqlite";
    store.backupTo(backup);
    expectException<std::runtime_error>([&] { store.backupTo(backup); });
    store.storeProperties({{device.local_id, {{"on_off", false}}}}, 2);
    require(store.loadProperties(2).at(device.local_id).size() == 1,
            "Replacement removes obsolete properties");

    DeviceStateStore restored(backup);
    smart_home::SQLiteDeviceStorage restored_inventory(backup.string());
    require(restored.loadProperties(1) == original,
            "Backup keeps the original revision and values");
    require(restored_inventory.findByPhysicalId("test", "driver") == device,
            "Backup keeps inventory records and IDs");
    require(restored_inventory.insert("another", "driver", "").local_id >
                device.local_id,
            "Backup keeps the inventory sequence");
    require(!fs::exists(backup.string() + "-wal") &&
                !fs::exists(backup.string() + "-journal"),
            "Backup is a standalone SQLite database");
}

} // namespace

int main() {
    try {
        roundTripAndRollback();
        require(nextRevision(0) == 1, "Revision advances");
        expectException<std::runtime_error>([] { nextRevision(-1); });
        expectException<std::runtime_error>(
            [] { nextRevision(std::numeric_limits<Revision>::max()); });
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
