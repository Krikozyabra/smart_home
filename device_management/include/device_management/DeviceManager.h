#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"
#include <string>

namespace smart_home {

class DeviceManager {
  private:
    IDeviceStorage &storage;

  public:
    explicit DeviceManager(IDeviceStorage &storage);

    DeviceRecord getOrCreate(const std::string &physical_id,
                             const std::string &driver_id,
                             const std::string &default_name);
};

} // namespace smart_home
