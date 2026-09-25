#include "discovery/SimulatedDiscoverySource.h"
#include "discovery/entities/DiscoveredDevice.h"

#include <string_view>
#include <utility>
#include <vector>

namespace smart_home {

SimulatedDiscoverySource::SimulatedDiscoverySource(
    std::vector<DiscoveredDevice> sim_devices)
    : cache(std::move(sim_devices)) {}

std::vector<DiscoveredDevice> SimulatedDiscoverySource::scan() { return cache; }

std::string_view SimulatedDiscoverySource::get_class_name() const {
    return "SimulatedSource";
};

} // namespace smart_home
