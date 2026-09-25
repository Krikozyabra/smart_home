#pragma once

#include "discovery/entities/DiscoveredDevice.h"

#include <string_view>
#include <vector>

namespace smart_home {

class IDeviceDiscoverySource {
  public:
    virtual ~IDeviceDiscoverySource() = default;

    virtual std::vector<DiscoveredDevice> scan() = 0;
    virtual std::string_view get_class_name() const = 0;
};

} // namespace smart_home
