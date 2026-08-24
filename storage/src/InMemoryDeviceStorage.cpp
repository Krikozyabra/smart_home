#include "storage/InMemoryDeviceStorage.h"
#include "storage/DeviceRecord.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace smart_home {

DeviceRecord InMemoryDeviceStorage::insert(const std::string &physical_id,
                                           const std::string &driver_id,
                                           const std::string &name) {
    if (physical_id.empty())
        throw std::invalid_argument("Physical id must not be empty");
    if (driver_id.empty())
        throw std::invalid_argument("Driver id must not be empty");

    const DeviceKey key{physical_id, driver_id};
    auto iterator = records.find(key);
    if (iterator != records.end())
        throw std::invalid_argument("Given physical id already in memory");

    DeviceRecord new_device{next_local_id, physical_id, driver_id, name};
    records.emplace(key, new_device);
    ++next_local_id;
    return new_device;
}

std::optional<DeviceRecord>
InMemoryDeviceStorage::findByPhysicalId(const std::string &physical_id,
                                        const std::string &driver_id) const {
    if (physical_id.empty())
        throw std::invalid_argument("Physical id must not be empty");
    if (driver_id.empty())
        throw std::invalid_argument("Driver id must not be empty");

    auto iterator = records.find({physical_id, driver_id});
    if (iterator == records.end())
        return std::nullopt;
    return iterator->second;
}

std::vector<DeviceRecord> InMemoryDeviceStorage::getAll() const {
    std::vector<DeviceRecord> result;
    for (const auto &iter : records) {
        result.push_back(iter.second);
    }
    return result;
}

} // namespace smart_home
