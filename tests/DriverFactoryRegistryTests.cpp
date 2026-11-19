#include "TestUtil.h"
#include "drivers/DeviceCreator.h"
#include "registry/DriverFactoryRegistry.h"

#include <stdexcept>

int main() {
    using namespace smart_home;
    DriverFactoryRegistry registry;
    require(registry.size() == 0, "Initial registry must be empty");
    require(registry.find("unknown") == nullptr,
            "An unknown protocol has no creator");

    expectException<std::invalid_argument>(
        [&] { registry.add("simulated", nullptr); });
    expectException<std::invalid_argument>(
        [&] { registry.add("", createSimulatedDriver); });

    registry.add("simulated", createSimulatedDriver);
    const auto *creator = registry.find("simulated");
    require(registry.size() == 1 && creator,
            "A creator must be stored by protocol");
    const DeviceRecord record{1, "SIM-1", "simulated", "Device"};
    const auto driver = (*creator)(record);
    require(driver->device().getId() == record.local_id,
            "The registered callable must construct the requested device");

    expectException<std::runtime_error>(
        [&] { registry.add("simulated", createSimulatedDriver); });
    require(registry.find("simulated") == creator,
            "Duplicate registration must keep the creator");
}
