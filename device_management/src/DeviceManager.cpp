#include "device_management/DeviceManager.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <optional>

namespace smart_home {

DeviceManager::DeviceManager(IDeviceStorage &c_storage) : storage(c_storage) {}

DeviceRecord DeviceManager::getOrCreate(const std::string &physical_id,
                                        const std::string &driver_id,
                                        const std::string &default_name) {
    auto record = storage.findByPhysicalId(physical_id, driver_id);
    if (record.has_value())
        return record.value();
    return storage.insert(physical_id, driver_id, default_name);
}

} // namespace smart_home
