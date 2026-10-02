#pragma once

#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <stdexcept>
#include <string>

namespace smart_home {

class UnsupportedDriverError : public std::invalid_argument {
  public:
    explicit UnsupportedDriverError(const std::string &message)
        : std::invalid_argument(message) {}
};

class DeviceManager {
  private:
    IDeviceStorage &storage;
    const DriverFactoryRegistry &factory_registry;
    DeviceRegistry &device_registry;

    DeviceRecord getOrCreate(const std::string &physical_id,
                             const std::string &driver_id,
                             const std::string &default_name);

  public:
    DeviceManager(IDeviceStorage &storage,
                  const DriverFactoryRegistry &factory_registry,
                  DeviceRegistry &device_registry);

    std::size_t restoreStoredDevices();

    DeviceRecord registerDiscoveredDevice(const std::string &physical_id,
                                          const std::string &driver_id,
                                          const std::string &default_name);
};

} // namespace smart_home
