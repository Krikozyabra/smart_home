#include "command/Command.h"
#include "command/Value.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "execution/CommandDispatcher.h"
#include "registry/DeviceRegistry.h"

#include "logging/Logging.h"

#include <stdexcept>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace {
int run() {
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

} // namespace

int main(int argc, char* argv[]) {
    namespace logging = smart_home::logging;
    logging::Config config;
    try {
        config = logging::resolveConfig(argc, argv, std::cerr);
    } catch (const std::invalid_argument& error) {
        std::cerr << "Logging configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        logging::Session session(config);
        logging::log(logging::Level::Info, "Main", "Application started.");
        try {
            const int result = run();
            logging::log(logging::Level::Info, "Main", "Application finished.");
            return result;
        } catch (const std::exception&) {
            // Exception text may contain physical addresses or private payloads.
            logging::log(logging::Level::Critical, "Main", "Unhandled application failure; exiting.");
            return 1;
        } catch (...) {
            logging::log(logging::Level::Critical, "Main", "Unknown application failure; exiting.");
            return 1;
        }
    } catch (...) {
        std::cerr << "Cannot initialize logging configuration.\n";
        return 2;
    }
}
