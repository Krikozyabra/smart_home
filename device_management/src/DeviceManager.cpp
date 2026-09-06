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
    std::vector<DeviceRecord> all_records = storage.getAll();
    std::vector<std::unique_ptr<IDeviceDriver>> drivers_for_registration;
    for (const auto &record : all_records) {
        IDeviceDriver *device_driver = device_registry.find(record.local_id);
        if (device_driver != nullptr)
            continue;
        auto *factory = factory_registry.find(record.driver_id);
        if (factory == nullptr)
            throw std::runtime_error("No factory registered for driver id = '" +
                                     record.driver_id + "' found");
        auto driver = factory->create(record);
        drivers_for_registration.push_back(std::move(driver));
    }
    for (auto &driver : drivers_for_registration)
        device_registry.add(std::move(driver));
    return drivers_for_registration.size();
}

DeviceRecord
DeviceManager::registerDiscoveredDevice(const std::string &physical_id,
                                        const std::string &driver_id,
                                        const std::string &default_name) {
    auto *factory = factory_registry.find(driver_id);
    if (factory == nullptr)
        throw std::invalid_argument("The driver '" + driver_id +
                                    "' is not supporting");
    DeviceRecord discovered_device =
        getOrCreate(physical_id, driver_id, default_name);
    IDeviceDriver *device_driver =
        device_registry.find(discovered_device.local_id);
    if (device_driver == nullptr) {
        auto driver = factory->create(discovered_device);
        if (driver == nullptr)
            throw std::logic_error(
                "DeviceManager: factory did not create correct driver");
        device_registry.add(std::move(driver));
    }
    return discovered_device;
}

} // namespace smart_home
