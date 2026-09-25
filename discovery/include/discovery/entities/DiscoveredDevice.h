#pragma once

#include <string>
namespace smart_home {

struct DiscoveredDevice {
    std::string physical_id;
    std::string driver_id;
    std::string default_name;

    inline bool operator==(const DiscoveredDevice &right) const{
        return (physical_id == right.physical_id) &&
               (driver_id == right.driver_id) &&
               (default_name == right.default_name);
    }
};

} // namespace smart_home
