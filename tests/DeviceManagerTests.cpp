#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/InMemoryDeviceStorage.h"
#include <memory>
#include <stdexcept>

int main() {
    {
        smart_home::InMemoryDeviceStorage storage;
        smart_home::DriverFactoryRegistry factories;
        factories.add(
            std::make_unique<smart_home::SimulatedLightDriverFactory>());
        smart_home::DeviceRegistry devices;

        smart_home::DeviceManager manager(storage, factories, devices);

        auto device_record = manager.registerDiscoveredDevice(
            "SIM-LIGHT-001", "simulated.light", "New Light");
        require(storage.getAll().size() == 1,
                "After registration device must be stored in storage");
        require(storage
                    .findByPhysicalId(device_record.physical_id,
                                      device_record.driver_id)
                    .has_value(),
                "Registered device must be found in presistence storage");
        require(devices.size() == 1,
                "Registered device must create runtime object");
        require(devices.find(device_record.local_id)->device().getName() ==
                    device_record.name,
                "Registered device must be found in runtime storage");
        auto found_record = manager.registerDiscoveredDevice(
            "SIM-LIGHT-001", "simulated.light", "Here we go");
        require(found_record.name == device_record.name,
                "Repeating registration for already registered device must "
                "return stored record");

        expectException<std::invalid_argument>([&] {
            manager.registerDiscoveredDevice("SIM-ROBOT-001", "simulated.robot",
                                             "New robot");
        });
        require(storage.getAll().size() == 1,
                "Storage should not change state after error");
    }
    {
        smart_home::InMemoryDeviceStorage storage;
        auto record1 = storage.insert("SIM-LIGHT-001", "simulated.light",
                                      "Default Light1");
        auto record2 = storage.insert("SIM-LIGHT-002", "simulated.light",
                                      "Default Light2");
        auto record3 = storage.insert("SIM-LIGHT-003", "simulated.light",
                                      "Default Light3");
        smart_home::DriverFactoryRegistry factories;
        smart_home::DeviceRegistry devices;

        smart_home::DeviceManager manager(storage, factories, devices);
        expectException<std::runtime_error>(
            [&] { manager.restoreStoredDevices(); });
        require(
            devices.size() == 0,
            "After exception size of device registry must be same as initial");
        factories.add(
            std::make_unique<smart_home::SimulatedLightDriverFactory>());
        require(manager.restoreStoredDevices() == 3,
                "Manager must restore all stored devices");

        auto *restored_driver = devices.find(record1.local_id);

        require(restored_driver != nullptr,
                "Restored driver must be registered by stored id");

        require(restored_driver->device().getName() == record1.name,
                "Restored device must preserve stored name");

        require(manager.restoreStoredDevices() == 0,
                "Repeated restoration must restore no new devices");

        require(devices.size() == 3,
                "Repeated restoration must not duplicate drivers");

        auto recrod4 =
            storage.insert("SIM-ROBOT-001", "unknown.driver", "New Robot");
        smart_home::DeviceRegistry devices_for_error;
        smart_home::DeviceManager manager_for_error(storage, factories,
                                                    devices_for_error);
        expectException<std::runtime_error>(
            [&] { manager_for_error.restoreStoredDevices(); });

        require(devices_for_error.size() == 0,
                "One device with unknown driver must cancel whole restoration");
    }
}
