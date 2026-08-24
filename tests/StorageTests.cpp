#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "storage/DeviceRecord.h"
#include "storage/InMemoryDeviceStorage.h"

#include <optional>
#include <stdexcept>

int main() {
    smart_home::InMemoryDeviceStorage storage;
    require(storage.getAll().empty(), "Initial storage must be empty");
    require(storage.findByPhysicalId("Physical id", "Driver id") ==
                std::nullopt,
            "Unstored physical id must return nullopt");

    require(storage.insert("AC:12:DD", "wifi", "Light") ==
                smart_home::DeviceRecord{0, "AC:12:DD", "wifi", "Light"},
            "storage.insert must return correct inserted object");
    require(storage.findByPhysicalId("AC:12:DD", "wifi") ==
                smart_home::DeviceRecord{0, "AC:12:DD", "wifi", "Light"},
            "finding by physical id must return correct record");
    require(storage.insert("AC:12:DD", "zigbee", "TemperatureSensor") ==
                smart_home::DeviceRecord{1, "AC:12:DD", "zigbee",
                                         "TemperatureSensor"},
            "storage.insert must insert next object with another local_id");
    require(storage.findByPhysicalId("AC:12:DD", "bluetooth") == std::nullopt,
            "finding by physical id must take driver id into account");
    expectException<std::invalid_argument>(
        [&] { storage.insert("AC:12:DD", "zigbee", "Duplicate TS"); });
    expectException<std::invalid_argument>(
        [&] { storage.insert("", "wifi", "Empty physical_id"); });
    expectException<std::invalid_argument>(
        [&] { storage.insert("D0:CC:00", "", "Empty driver_id"); });
    expectException<std::invalid_argument>(
        [&] { storage.findByPhysicalId("", "wifi"); });
    expectException<std::invalid_argument>(
        [&] { storage.findByPhysicalId("D0:CC:00", ""); });

    require(storage.getAll().size() == 2, "All records must be in memory");

    smart_home::InMemoryDeviceStorage storage2;
    smart_home::DeviceManager manager(storage2);
    auto first = manager.getOrCreate("12:DD:00", "wifi", "Light1");
    auto second = manager.getOrCreate("12:DD:00", "wifi", "Light_default");
    require(first == second, "getOrCreate must return stored record if "
                             "physical and driver id equal");
    require(storage2.getAll().size() == 1,
            "DeviceManager should insert only one record");
    require(second.name == "Light1",
            "getOrCreate must return stored first record");
}
