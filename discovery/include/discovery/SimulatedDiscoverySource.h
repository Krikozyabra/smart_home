#pragma once

#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include <string_view>
#include <vector>

namespace smart_home {

class SimulatedDiscoverySource : public IDeviceDiscoverySource {
  private:
    std::vector<DiscoveredDevice> cache;

  public:
    SimulatedDiscoverySource(std::vector<DiscoveredDevice>);

    std::vector<DiscoveredDevice> scan() override;

    std::string_view get_class_name() const override;
};

} // namespace smart_home
