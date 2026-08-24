#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "storage/DeviceRecord.h"
#include "storage/SQLiteDeviceStorage.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
class FileRAII {
    fs::path file_path;

  public:
    FileRAII(const std::string c_file_path) : file_path(c_file_path) {}
    ~FileRAII() {
        if (fs::exists(file_path))
            fs::remove(file_path);
    }
    const std::string getStringPath() const { return file_path.string(); }
};

int main() {
    try {
        FileRAII db_file("test.db");
        std::optional<smart_home::DeviceRecord> saved_wifi;
        {
            smart_home::SQLiteDeviceStorage storage(db_file.getStringPath());
            require(storage.getAll().empty(), "Initial storage must be empty");
            require(
                storage.findByPhysicalId("Physical id", "Driver id") ==
                    std::nullopt,
                "Unstored pair physical id + driver id must return nullopt");
            saved_wifi = storage.insert("AC:12:DD", "wifi.light", "Light");
            require(saved_wifi->local_id > 0,
                    "Inserted device must have a positive local id");
            require(saved_wifi->physical_id == "AC:12:DD",
                    "Inserted record must contain physical id");
            require(saved_wifi->driver_id == "wifi.light",
                    "Inserted record must contain driver id");
            require(saved_wifi->name == "Light",
                    "Inserted record must contain name");
            require(storage.findByPhysicalId("AC:12:DD", "wifi.light") ==
                        saved_wifi,
                    "finding by physical id must return correct record");

            auto ts_zigbee = storage.insert(
                "AC:12:DD", "zigbee.temperature_sensor", "TemperatureSensor");
            require(
                ts_zigbee.local_id != saved_wifi->local_id,
                "storage.insert must insert next object with another local_id");
            expectException<std::invalid_argument>([&] {
                storage.insert("AC:12:DD", "zigbee.temperature_sensor",
                               "Duplicate TS");
            });
            require(storage.getAll().size() == 2,
                    "All records must be in memory");
            auto bluetooth_light = storage.insert("AC:12:DD", "bluetooth.light",
                                                  "Лампа's на кириллице");
            require(bluetooth_light.name == "Лампа's на кириллице",
                    "SQLite must preserve UTF-8 text and apostrophes");
        }
        {
            smart_home::SQLiteDeviceStorage storage(db_file.getStringPath());
            smart_home::DeviceManager manager(storage);
            auto restored_wifi = manager.getOrCreate(
                "AC:12:DD", "wifi.light", "default random light name");
            require(restored_wifi == *saved_wifi,
                    "Stored device must survive database reopening");
            require(restored_wifi.name == "Light",
                    "Rediscovery must not overwrite stored name");
            require(storage.getAll().size() == 3,
                    "getorcreate must not create duplicates");
        }
    } catch (const std::exception &ec) {
        std::cerr << "Tests failed with error: " << ec.what();
        return 1;
    }
}
