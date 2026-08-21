#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "registry/DeviceRegistry.h"

#include <cassert>
#include <stdexcept>

template <typename Exception, typename Function>
void expectException(Function function) {
    bool caught = false;

    try {
        function();
    } catch (const Exception &) {
        caught = true;
    }

    assert(caught);
}

int main() {
    smart_home::DeviceRegistry registry;

    registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
        0, "AC:12:BD", "Light1", false, 50, Color{100, 50, 200}));
    registry.add(std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
        1, "D1:A0:67", "TemperatureSensor in bath", 23.0));

    // Tests for registry
    assert(registry.size() == 2);
    assert(registry.find(0) != nullptr);
    assert(registry.find(1) != nullptr);
    assert(registry.find(100) == nullptr);

    expectException<std::invalid_argument>([&] { registry.add(nullptr); });
    assert(registry.size() == 2);

    expectException<std::runtime_error>([&] {
        registry.add(
            std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
                1, "D1:A0:67", "TemperatureSensor in bath", 23.0));
    });
    assert(registry.size() == 2);

    return 0;
}
