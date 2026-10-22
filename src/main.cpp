#include "command/Command.h"
#include "command/Value.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "execution/CommandDispatcher.h"
#include "registry/DeviceRegistry.h"

#include "logging/Logging.h"
#include "ontology/LadybugOntologyStorage.h"

#include <stdexcept>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>
#include <string>

namespace {
smart_home::ontology::OntologyConfig ontologyArguments(
        int argc, char* argv[], std::vector<const char*>& logging_arguments) {
    auto config = smart_home::ontology::configFromEnvironment();
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument.rfind("--ontology-", 0) != 0) {
            logging_arguments.push_back(argv[index]);
            continue;
        }
        const auto equal = argument.find('=');
        const auto name = argument.substr(0, equal);
        if (name != "--ontology-path" && name != "--ontology-ram-dir" &&
            name != "--ontology-sync-seconds" && name != "--ontology-allow-disk-fallback")
            throw std::invalid_argument("Unknown ontology argument.");
        std::string value;
        if (equal != std::string::npos) value = argument.substr(equal + 1);
        else {
            if (++index >= argc) throw std::invalid_argument("Missing ontology argument value.");
            value = argv[index];
        }
        if (name == "--ontology-path") config.persistent_path = value;
        if (name == "--ontology-ram-dir") config.ram_workspace_base = value;
        if (name == "--ontology-sync-seconds") config.sync_interval = smart_home::ontology::parseSyncInterval(value);
        if (name == "--ontology-allow-disk-fallback") {
            if (value != "0" && value != "1") throw std::invalid_argument("Ontology fallback must be 0 or 1.");
            config.allow_disk_fallback = value == "1";
        }
    }
    smart_home::ontology::validateConfig(config);
    return config;
}

int run(const smart_home::ontology::OntologyConfig& ontology_config) {
    smart_home::DeviceRegistry registry;

    smart_home::CommandDispatcher command_dispatcher(registry);

    registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
        0, "AC:12:BD", "Light1", false, 50, Color{100, 50, 200}));
    registry.add(std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
        1, "D1:A0:67", "TemperatureSensor in bath", 23.0));

    smart_home::ontology::LadybugOntologyStorage ontology(ontology_config);
    if (!ontology.getRoom("living_room")) ontology.addRoom({"living_room", "Гостиная"});
    if (!ontology.getRoom("bathroom")) ontology.addRoom({"bathroom", "Ванная комната"});
    for (smart_home::DeviceId id : {0, 1}) {
        const auto& descriptor = registry.find(id)->descriptor();
        if (!ontology.getDevice(id)) {
            ontology.addDevice({id, descriptor.getName(), id == 0 ? "light" : "temperature_sensor"});
            ontology.assignDeviceToRoom(id, id == 0 ? "living_room" : "bathroom");
        }
        for (const auto& capability : descriptor.getCapabilities()) {
            ontology.addAbility({capability.getId(), capability.getName(), id == 0 ? "lighting" : "climate"});
            ontology.addDeviceAbility(id, capability.getId());
        }
    }
    const auto bathroom_devices = ontology.getDevicesInRoom("bathroom");
    smart_home::logging::log(smart_home::logging::Level::Debug, "Ontology",
        "Bathroom device count: " + std::to_string(bathroom_devices.size()));

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

    ontology.sync();
    ontology.close(); // Surface a failed final snapshot at the application's handling boundary.
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    namespace logging = smart_home::logging;
    logging::Config config;
    smart_home::ontology::OntologyConfig ontology_config;
    std::vector<const char*> logging_arguments{argv[0]};
    try {
        ontology_config = ontologyArguments(argc, argv, logging_arguments);
    } catch (const std::invalid_argument& error) {
        std::cerr << "Ontology configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        config = logging::resolveConfig(static_cast<int>(logging_arguments.size()), logging_arguments.data(), std::cerr);
    } catch (const std::invalid_argument& error) {
        std::cerr << "Logging configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        logging::Session session(config);
        logging::log(logging::Level::Info, "Main", "Application started.");
        try {
            const int result = run(ontology_config);
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
