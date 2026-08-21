#include "command/Command.h"
#include "command/Value.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "execution/CommandDispatcher.h"
#include "registry/DeviceRegistry.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

int main(int argc, char *argv[]) {
    smart_home::DeviceRegistry registry;

    smart_home::CommandDispatcher command_dispatcher(registry);

    registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
        0, "AC:12:BD", "Light1", false, 50, Color{100, 50, 200}));
    registry.add(std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
        1, "D1:A0:67", "TemperatureSensor in bath", 23.0));

    smart_home::Command brightness_set(0, "brightness.set",
                                       std::vector<smart_home::Value>{75});

    smart_home::Command brightness_get(0, "brightness.get", {});

    smart_home::Command temperature_get(1, "temperature.get",
                                        std::vector<smart_home::Value>{});

    std::optional<smart_home::Value> result =
        command_dispatcher.execute(temperature_get);
    if (result != std::nullopt)
        std::cout << "Temperature is " << std::get<double>(*result)
                  << std::endl;

    result = command_dispatcher.execute(brightness_get);
    if (result != std::nullopt)
        std::cout << "Brightness is "
                  << static_cast<int>(std::get<int>(*result)) << std::endl;

    command_dispatcher.execute(brightness_set);

    result = command_dispatcher.execute(brightness_get);
    std::cout << "New brightness is "
              << static_cast<int>(std::get<int>(*result)) << std::endl;

    return 0;
}
