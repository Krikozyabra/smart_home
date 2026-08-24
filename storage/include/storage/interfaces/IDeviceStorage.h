#pragma once

#include "storage/DeviceRecord.h"

#include <optional>
#include <string>
#include <vector>

namespace smart_home {

class IDeviceStorage {
  public:
    virtual ~IDeviceStorage() = default;

    virtual std::optional<DeviceRecord>
    findByPhysicalId(const std::string &driver_id,
                     const std::string &physical_id) const = 0;

    virtual DeviceRecord insert(const std::string &physical_id,
                                const std::string &driver_id,
                                const std::string &name) = 0;

    virtual std::vector<DeviceRecord> getAll() const = 0;
};

} // namespace smart_home
