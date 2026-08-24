#pragma once

#include "common/DeviceId.h"

#include <string>

namespace smart_home {
struct DeviceRecord {
    DeviceId local_id;
    std::string physical_id;
    std::string driver_id;
    std::string name;

    inline bool operator==(const DeviceRecord &other) const noexcept {
        return local_id == other.local_id && physical_id == other.physical_id &&
               driver_id == other.driver_id && name == other.name;
    }
};

} // namespace smart_home
