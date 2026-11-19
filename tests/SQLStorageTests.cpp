#include "TestUtil.h"
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

        smart_home::IDeviceStorage &inventory = storage;
        inventory.close();
        inventory.close();
        expectException<std::logic_error>([&] { inventory.getAll(); });
        expectException<std::logic_error>(
            [&] { inventory.findByPhysicalId("AC:12:DD", "wifi.light"); });
        expectException<std::logic_error>(
            [&] { inventory.insert("new", "wifi.light", "Name"); });
        try {
            inventory.insert("", "", "");
            require(false, "Closed storage must reject invalid insert arguments");
        } catch (const std::logic_error &error) {
            require(std::string(error.what()) == "Storage is closed.",
                    "Closed state is checked before insert arguments");
        }
        try {
            inventory.findByPhysicalId("", "");
            require(false, "Closed storage must reject invalid lookup arguments");
        } catch (const std::logic_error &error) {
            require(std::string(error.what()) == "Storage is closed.",
                    "Closed state is checked before lookup arguments");
        }
        try {
            inventory.dumpToFile("");
            require(false, "Closed storage must reject dump");
        } catch (const std::logic_error &error) {
            require(std::string(error.what()) == "Storage is closed.",
                    "Closed state is checked before dump path validation");
        }

        smart_home::SQLiteDeviceStorage reopened(db_file.getStringPath());
        require(reopened.findByPhysicalId("AC:12:DD", "wifi.light") == saved_wifi,
                "File database can be reopened before closed object destruction");
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
    smart_home::SQLiteDeviceStorage empty(":memory:");
    const auto legacy = empty.insert("sim-1", "simulated.light", "Light");
    empty.insert("sim-2", "simulated.temperature_sensor", "Sensor");
    empty.insert("sim-2", "simulated", "Conflicting record");
    const std::vector<std::string> old_ids{"simulated.light", "simulated.temperature_sensor"};
    expectException<std::runtime_error>([&] { empty.replaceDriverIds(old_ids, "simulated"); });
    require(empty.findByPhysicalId("sim-1", "simulated.light") == legacy &&
                empty.findByPhysicalId("sim-2", "simulated.temperature_sensor").has_value(),
            "A migration conflict must roll back the entire update");
    empty.close();
    empty.close();
    expectException<std::logic_error>([&] { empty.getAll(); });
}
