#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "storage/DeviceRecord.h"
#include "storage/SQLiteDeviceStorage.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
class FileRAII {
    fs::path file_path;

  public:
    FileRAII(const std::string c_file_path) : file_path(c_file_path) {
        std::error_code code;
        fs::remove(file_path, code);
    }
    ~FileRAII() noexcept {
        std::error_code ignored_error;
        fs::remove(file_path, ignored_error);
    }
    const std::string getStringPath() const { return file_path.string(); }
};

int main() {
    FileRAII db_file("test.db");
    std::optional<smart_home::DeviceRecord> saved_wifi;
    {
        smart_home::SQLiteDeviceStorage storage(db_file.getStringPath());
        require(storage.getAll().empty(), "Initial storage must be empty");
        require(storage.findByPhysicalId("Physical id", "Driver id") ==
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

        auto ts_zigbee = storage.insert("AC:12:DD", "zigbee.temperature_sensor",
                                        "TemperatureSensor");
        require(ts_zigbee.local_id != saved_wifi->local_id,
                "storage.insert must insert next object with another local_id");
        expectException<std::invalid_argument>([&] {
            storage.insert("AC:12:DD", "zigbee.temperature_sensor",
                           "Duplicate TS");
        });
        require(storage.getAll().size() == 2, "All records must be in memory");
        auto bluetooth_light = storage.insert("AC:12:DD", "bluetooth.light",
                                              "Лампа's на кириллице");
        require(bluetooth_light.name == "Лампа's на кириллице",
                "SQLite must preserve UTF-8 text and apostrophes");
    }
    {
        smart_home::SQLiteDeviceStorage storage(db_file.getStringPath());

        const auto restored =
            storage.findByPhysicalId("AC:12:DD", "wifi.light");

        require(restored.has_value(),
                "Stored record must survive database reopening");

        require(restored == saved_wifi,
                "Reopened database must return the same record");

        require(storage.getAll().size() == 3,
                "All records must survive database reopening");
    }
}
