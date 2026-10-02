#include "discovery/SimulatedDiscoverySource.h"
#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/entities/DiscoveryError.h"

#include <string_view>
#include <utility>
#include <vector>

namespace smart_home {

SimulatedDiscoverySource::SimulatedDiscoverySource(
    std::vector<DiscoveredDevice> sim_devices, bool c_is_error)
    : cache(std::move(sim_devices)), is_error(c_is_error) {}

std::vector<DiscoveredDevice> SimulatedDiscoverySource::scan() {
    if (is_error)
        throw DiscoveryScanError{"Some simulated error in simulated source"};
    return cache;
}

std::string_view SimulatedDiscoverySource::get_class_name() const {
    return "SimulatedSource";
};

} // namespace smart_home
