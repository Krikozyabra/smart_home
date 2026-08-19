#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "devices/interfaces/IBrightness.h"
#include "devices/interfaces/ITemperature.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "execution/CommandDispatcher.h"
#include "registry/DeviceRegistry.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

int main(int argc, char *argv[]) {
    smart_home::DeviceRegistry registry;

    smart_home::CommandDispatcher command_dispatcher(registry);

    registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
        0, "AC:12:BD", "Light1", false, 50, Color{100, 50, 200}));
    registry.add(std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
        1, "D1:A0:67", "TemperatureSensor in bath", 23.0));

    smart_home::Command brightness_command(0, "brightness.set",
                                           std::vector<smart_home::Value>{75});
    assert(brightness_command.getDeviceId() == 0);
    assert(brightness_command.getArguments().size() == 1);
    assert(brightness_command.getOperationId() == "brightness.set");

    assert(std::holds_alternative<int>(brightness_command.getArguments()[0]));
    assert(std::get<int>(brightness_command.getArguments()[0]) == 75);


    smart_home::Command temperature_get(1, "temperature.get",
                                        std::vector<smart_home::Value>{});
    assert(temperature_get.getArguments().size() == 0);

    smart_home::IDeviceDriver *driver = registry.find(1);

    if (driver != nullptr) {
        smart_home::Device &device = driver->device();

        if (auto *temperature =
                dynamic_cast<smart_home::ITemperature *>(&device)) {
            std::cout << "Temperature is " << temperature->getTemperature()
                      << std::endl;
        }
    }

    driver = registry.find(0);

    if (driver != nullptr) {
        smart_home::Device &device = driver->device();
        auto *brightness = dynamic_cast<smart_home::IBrightness *>(&device);
        if (brightness) {
            std::cout << "Brightness is "
                      << static_cast<int>(brightness->getBrightness())
                      << std::endl;
        }
        command_dispatcher.execute(brightness_command);
        if (brightness) {
            std::cout << "New brightness is "
                      << static_cast<int>(brightness->getBrightness())
                      << std::endl;
        }
    }
    
    smart_home::Command get_command(0, "brightness.get", {});

    std::optional<smart_home::Value> result =
        command_dispatcher.execute(get_command);
    assert(result.has_value());
    assert(std::holds_alternative<int>(*result));
    assert(std::get<int>(*result) == 75);

    assert(registry.size() == 2);
    assert(registry.find(0) != nullptr);
    assert(registry.find(1) != nullptr);
    assert(registry.find(100) == nullptr);

    try {
        registry.add(nullptr);
    } catch (const std::invalid_argument &) {
        std::cout << "Nullptr catched\n";
        assert(registry.size() == 2);
    }

    try {
        registry.add(
            std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
                1, "D1:A0:67", "TemperatureSensor in bath", 23.0));
    } catch (const std::runtime_error &) {
        std::cout << "Local id duplicate catched\n";
        assert(registry.size() == 2);
    }

    return 0;
}
