#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"
#include "registry/DriverFactoryRegistry.h"
#include "registry/DeviceRegistry.h"
#include <string>

namespace smart_home {

class DeviceManager {
private:
  IDeviceStorage &storage;
  const DriverFactoryRegistry &factory_registry;
  DeviceRegistry &device_registry;

public:
  DeviceManager(IDeviceStorage &storage,
                const DriverFactoryRegistry &factory_registry,
                DeviceRegistry &device_registry);

  DeviceRecord getOrCreate(const std::string &physical_id,
                           const std::string &driver_id,
                           const std::string &default_name);

  std::size_t restoreStoredDevices();
};

} // namespace smart_home
