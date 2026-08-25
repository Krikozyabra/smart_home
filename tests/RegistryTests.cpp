#include "TestUtil.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <stdexcept>

int main() {
    {
        smart_home::DeviceRegistry registry;

        registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
            0, "AC:12:BD", "Light1", false, 50, Color{100, 50, 200}));
        registry.add(
            std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
                1, "D1:A0:67", "TemperatureSensor in bath", 23.0));

        // Tests for registry
        require(registry.size() == 2,
                "DeviceRegistry must register all drivers");
        require(registry.find(0) != nullptr,
                "DeviceRegistry must find first driver");
        require(registry.find(1) != nullptr,
                "DeviceRegistry must find second driver");
        require(registry.find(100) == nullptr,
                "DeviceRegistry must return nullptr for unregistered local id");

        expectException<std::invalid_argument>([&] { registry.add(nullptr); });
        require(
            registry.size() == 2,
            "DeviceRegistry must not edit registry after error with nullptr");

        expectException<std::runtime_error>([&] {
            registry.add(
                std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
                    1, "D1:A0:67", "TemperatureSensor in bath", 23.0));
        });
        require(registry.size() == 2,
                "DeviceRegistry: duplicate driver must not change registry");
    }
    {
        smart_home::DriverFactoryRegistry registry;
        require(registry.size() == 0,
                "DriverFactoryRegistry: initial registry must be empty");
        expectException<std::invalid_argument>([&] { registry.add(nullptr); });
        registry.add(
            std::make_unique<smart_home::SimulatedLightDriverFactory>());
        require(
            registry.size() == 1,
            "DriverFactoryRegistry: adding correct factory must increase size");
        require(registry.find("simulated.light") != nullptr,
                "DriverFactoryRegistry: finding correct driver id must return "
                "not nullptr");
        const auto &const_registry = registry;
        require(registry.find("simulated.light") != nullptr,
                "DriverFactoryRegistry: const registry must find registered "
                "factory");
        require(registry.find("some.test") == nullptr,
                "DriverFactoryRegistry: finding unregistered driver id must "
                "return nullptr");
        expectException<std::runtime_error>([&] {
            registry.add(
                std::make_unique<smart_home::SimulatedLightDriverFactory>());
        });
        require(registry.size() == 1, "DriverFactoryRegistry: duplicate "
                                      "factory must not change registry");

        auto *driver_factory = registry.find("simulated.light");
        require(driver_factory != nullptr,
                "DriverFactoryRegistry: registered factory must be found");

        smart_home::DeviceRecord test_record{0, "AC:12:BD", "simulated.light",
                                             "Light1"};

        auto driver = driver_factory->create(test_record);
        require(driver->device().getName() == "Light1",
                "All interfaces must work correctly");
    }
    return 0;
}
