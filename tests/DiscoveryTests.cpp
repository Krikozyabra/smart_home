#include "TestUtil.h"

#include "discovery/SimulatedDiscoverySource.h"
#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include <vector>

int main() {
    smart_home::SimulatedDiscoverySource empty_sds(
        std::vector<smart_home::DiscoveredDevice>{}, false);
    require(empty_sds.scan().size() == 0, "Initial source must be empty");

    std::vector<smart_home::DiscoveredDevice> devices_for_sds;
    devices_for_sds.push_back(smart_home::DiscoveredDevice{
        "SIM-LIGHT-001", "simulated.light", "New Light"});
    devices_for_sds.push_back(
        smart_home::DiscoveredDevice{"SIM-TS-001", "simulated.ts", "New TS"});
    smart_home::SimulatedDiscoverySource full_sds(devices_for_sds, false);
    require(devices_for_sds == full_sds.scan(),
            "Scan should return cached devices");
    smart_home::IDeviceDiscoverySource *source_interface =
        dynamic_cast<smart_home::IDeviceDiscoverySource *>(&full_sds);
    require(source_interface != nullptr,
            "Simulated source must be derrived from interface");
    require(source_interface->scan() == devices_for_sds,
            "Calling scan from interface must return same device list");
}
