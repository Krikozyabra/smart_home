#include "device_management/DeviceManager.h"
#include "drivers/factories/IDeviceDriverFactory.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace smart_home {
DeviceManager::DeviceManager(IDeviceStorage &c_storage,
                             const DriverFactoryRegistry &c_factory_registry,
                             DeviceRegistry &c_device_registry)
    : storage(c_storage), factory_registry(c_factory_registry),
      device_registry(c_device_registry) {}
DeviceRecord DeviceManager::getOrCreate(const std::string &physical_id,
                                        const std::string &driver_id,
                                        const std::string &default_name) {
  auto record = storage.findByPhysicalId(physical_id, driver_id);
  if (record.has_value())
    return record.value();
  return storage.insert(physical_id, driver_id, default_name);
}
std::size_t DeviceManager::restoreStoredDevices() {
  std::size_t num_restored_devices = 0;
  std::vector<DeviceRecord> all_records = storage.getAll();
  for (const auto &record : all_records) {
    IDeviceDriver *device_driver = device_registry.find(record.local_id);
    if (device_driver != nullptr)
      continue;
    auto *factory = factory_registry.find(record.driver_id);
    if (factory == nullptr)
      throw std::runtime_error("No factory registered for driver id = '" +
                               record.driver_id + "' found");
    auto driver = factory->create(record);
    device_registry.add(std::move(driver));
    ++num_restored_devices;
  }
  return num_restored_devices;
}
} // namespace smart_home
