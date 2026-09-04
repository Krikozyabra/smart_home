#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/InMemoryDeviceStorage.h"
#include <stdexcept>

int main() {
  smart_home::InMemoryDeviceStorage storage;
  smart_home::DriverFactoryRegistry factories;
  smart_home::DeviceRegistry devices;

  smart_home::DeviceManager manager(storage, factories, devices);
  smart_home::DeviceRecord created_record =
      manager.getOrCreate("SIM-LIGHT-001", "simulated.light", "Default Light");
  require(created_record.local_id >= 0,
          "Try to getOrCreate unknown device must create device record in "
          "storage");
  smart_home::DeviceRecord founded_record = manager.getOrCreate(
      "SIM-LIGHT-001", "simulated.light", "New default name");
  require(created_record.name == founded_record.name,
          "Founded record must be exactly same as one created lastly");
  require(storage.getAll().size() == 1, "Storage must store only one record");
  expectException<std::runtime_error>([&] { manager.restoreStoredDevices(); });
  require(devices.size() == 0,
          "After exception size of device registry must be same as initial");
  factories.add(std::make_unique<smart_home::SimulatedLightDriverFactory>());
  require(manager.restoreStoredDevices() == 1,
          "Manager must restore all stored devices");

  auto *restored_driver = devices.find(created_record.local_id);

  require(restored_driver != nullptr,
          "Restored driver must be registered by stored id");

  require(restored_driver->device().getName() == created_record.name,
          "Restored device must preserve stored name");

  require(manager.restoreStoredDevices() == 0,
          "Repeated restoration must restore no new devices");

  require(devices.size() == 1,
          "Repeated restoration must not duplicate drivers");
}
