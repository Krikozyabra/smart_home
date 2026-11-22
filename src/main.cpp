/** @file main.cpp
 * @brief Композиция приложения, консольный цикл и фиксация полных поколений.
 */
#include "ConsoleInput.h"
#include "command/Command.h"
#include "device_management/DeviceManager.h"
#include "discovery/DiscoveryService.h"
#include "discovery/SimulatedDiscoverySource.h"
#include "drivers/CapabilityCatalog.h"
#include "drivers/DeviceCreator.h"
#include "events/EventBus.h"
#include "events/Reactor.h"
#include "execution/CommandDispatcher.h"
#include "logging/Logging.h"
#include "ontology/LadybugOntologyStorage.h"
#include "persistence/DeviceStateStore.h"
#include "persistence/GenerationManager.h"
#include "persistence/PersistenceConfig.h"
#include "persistence/SaveWorker.h"
#include "persistence/StateLock.h"
#include "platform/EventPipe.h"
#include "platform/SignalHandler.h"
#include "platform/TemporaryDirectory.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/SQLiteDeviceStorage.h"

#if defined(SMART_HOME_RUNTIME_TESTING) && SMART_HOME_RUNTIME_TESTING == 1
#include "Testing.h"
#endif

#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace smart_home;
namespace ev = smart_home::events;
namespace fs = std::filesystem;
namespace log = smart_home::logging;
namespace persist = smart_home::persistence;
using Ontology = ontology::LadybugOntologyStorage;
using persist::DevicePropertiesMap;
using persist::Revision;

struct ConsoleCommandEvent {
    std::string line;
};

struct StopRequestedEvent {};

template <class T> T integer(const std::string &text) {
    T result{};
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Expected a complete integer in range.");
    return result;
}

persist::PersistenceConfig
arguments(int argc, char *argv[],
          std::vector<const char *> &logging_arguments) {
    persist::PersistenceConfig config;
    if (const char *value = std::getenv("SMART_HOME_RUNTIME_STATE_DIR"))
        config.state_dir = value;
    if (const char *value = std::getenv("SMART_HOME_RUNTIME_RAM_DIR"))
        config.ram_dir = value;
    if (const char *value = std::getenv("SMART_HOME_RUNTIME_SNAPSHOT_CAPACITY"))
        config.capacity = integer<std::size_t>(value);
    // The standalone ontology destination/worker must never bypass CURRENT.
    for (const char *name :
         {"SMART_HOME_ONTOLOGY_PATH", "SMART_HOME_ONTOLOGY_RAM_DIR",
          "SMART_HOME_ONTOLOGY_SYNC_SECONDS",
          "SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK"})
        if (std::getenv(name))
            throw std::invalid_argument(
                "Standalone ontology settings conflict with runtime; use "
                "--runtime-* settings.");
    for (int i = 1; i < argc; ++i) {
        std::string argument(argv[i]);
        if (argument.rfind("--ontology-", 0) == 0)
            throw std::invalid_argument(
                "Standalone ontology settings conflict with runtime.");
        if (argument.rfind("--runtime-", 0) != 0) {
            logging_arguments.push_back(argv[i]);
            continue;
        }
        const auto equal = argument.find('=');
        const auto name = argument.substr(0, equal);
        if (name != "--runtime-state-dir" && name != "--runtime-ram-dir" &&
            name != "--runtime-snapshot-capacity")
            throw std::invalid_argument("Unknown runtime option.");
        std::string value;
        if (equal != std::string::npos)
            value = argument.substr(equal + 1);
        else {
            if (++i >= argc)
                throw std::invalid_argument("Missing runtime option value.");
            value = argv[i];
        }
        if (name == "--runtime-state-dir")
            config.state_dir = value;
        if (name == "--runtime-ram-dir")
            config.ram_dir = value;
        if (name == "--runtime-snapshot-capacity")
            config.capacity = integer<std::size_t>(value);
    }
    persist::validateConfig(config);
    return config;
}

IDeviceDriver &requireDriver(DeviceRegistry &registry, DeviceId id) {
    auto *driver = registry.find(id);
    if (!driver)
        throw std::runtime_error("Inventory driver missing.");
    return *driver;
}

std::string type(const IDeviceDriver &driver) {
    bool measures_temperature = false;
    for (const auto &capability : driver.descriptor().getCapabilities()) {
        const auto &id = capability.getId();
        if (id == "on_off" || id == "brightness")
            return "light";
        measures_temperature |= id == "temperature";
    }
    return measures_temperature ? "temperature_sensor" : "device";
}

Value decodeProperty(const persist::PropertyValue &saved) {
    return std::visit(
        [](const auto &value) -> Value {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::int64_t>) {
                if (value < std::numeric_limits<int>::min() ||
                    value > std::numeric_limits<int>::max())
                    throw std::runtime_error("Saved device integer out of range.");
                return static_cast<int>(value);
            } else {
                return value;
            }
        }, saved);
}

persist::PropertyValue encodeProperty(const Value &property) {
    return std::visit(
        [](const auto &value) -> persist::PropertyValue {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int>) {
                return static_cast<std::int64_t>(value);
            } else if constexpr (std::is_same_v<T, char>) {
                throw std::logic_error("Character properties are not supported by persistence.");
            } else {
                return value;
            }
        }, property);
}

const CapabilityCatalog::Definition &requireCapability(const std::string &id) {
    const auto *entry = CapabilityCatalog::find(id);
    if (!entry)
        throw std::runtime_error("Unknown runtime capability: " + id);
    return *entry;
}

void restoreProperties(SQLiteDeviceStorage &storage, DeviceRegistry &registry,
                       const DevicePropertiesMap &restored) {
    for (const auto &record : storage.getAll()) {
        auto &driver = requireDriver(registry, record.local_id);
        const auto &capabilities = driver.descriptor().getCapabilities();
        const auto device_values = restored.find(record.local_id);
        if (device_values == restored.end() && capabilities.empty())
            continue;
        if (device_values == restored.end())
            throw std::runtime_error("Incomplete saved device values.");
        const auto &saved = device_values->second;
        if (saved.size() != capabilities.size())
            throw std::runtime_error("Saved values disagree with device capabilities.");
        for (const auto &capability : capabilities) {
            const auto &capability_id = capability.getId();
            const auto &entry = requireCapability(capability_id);
            const auto found = saved.find(capability_id);
            if (found == saved.end())
                throw std::runtime_error("Missing saved device property.");
            const auto value = decodeProperty(found->second);
            // Earlier snapshots can contain finite temperatures outside the current range.
            const bool archived_temperature =
                capability_id == "temperature" && std::holds_alternative<double>(value);
            if (!archived_temperature && !entry.accepts(value))
                throw std::runtime_error("Invalid saved device property.");
            driver.device().setProperty(entry.key(), value);
        }
    }
}

DevicePropertiesMap capture(SQLiteDeviceStorage &storage, DeviceRegistry &registry) {
    DevicePropertiesMap result;
    for (const auto &record : storage.getAll()) {
        auto &driver = requireDriver(registry, record.local_id);
        auto &values = result[record.local_id];
        for (const auto &capability : driver.descriptor().getCapabilities()) {
            const auto &id = capability.getId();
            const auto value = requireCapability(id).read(driver.device());
            if (!value)
                throw std::logic_error("Missing runtime device property: " + id);
            values.emplace(id, encodeProperty(*value));
        }
    }
    return result;
}

bool prepareGraph(Ontology &graph) {
    bool changed = false;
    for (const auto &room :
         {ontology::RoomRecord{"living_room", "Гостиная"},
          ontology::RoomRecord{"bathroom", "Ванная комната"}})
        if (!graph.getRoom(room.id)) {
            graph.addRoom(room);
            changed = true;
        }
    for (const auto &ability :
         {ontology::AbilityRecord{"on_off", "Включение", "lighting"},
          ontology::AbilityRecord{"brightness", "Яркость", "lighting"},
          ontology::AbilityRecord{"color", "Цвет", "lighting"},
          ontology::AbilityRecord{"temperature", "Температура", "climate"},
          ontology::AbilityRecord{"humidity", "Влажность", "climate"},
          ontology::AbilityRecord{"contact", "Контакт", "security"},
          ontology::AbilityRecord{"battery", "Заряд", "power"},
          ontology::AbilityRecord{"power", "Мощность", "power"}})
        if (!graph.getAbility(ability.id)) {
            graph.addAbility(ability);
            changed = true;
        }
    return changed;
}

bool assignDefaultRooms(Ontology &graph, SQLiteDeviceStorage &storage,
                        DeviceRegistry &registry) {
    bool changed = false;
    for (const auto &record : storage.getAll())
        if (!graph.getDeviceRoom(record.local_id)) {
            const auto category = type(requireDriver(registry, record.local_id));
            graph.assignDeviceToRoom(record.local_id,
                                     category == "light" ? "living_room" : "bathroom");
            changed = true;
        }
    return changed;
}

using GraphSnapshot = std::map<DeviceId, ontology::DeviceOntologyDetails>;

GraphSnapshot snapshotGraph(Ontology &graph) {
    GraphSnapshot snapshot;
    for (const auto &device : graph.getAllDevices())
        snapshot.emplace(device.id, graph.getDeviceDetails(device.id).value());
    return snapshot;
}

void validateGraph(Ontology &graph, SQLiteDeviceStorage &storage,
                   DeviceRegistry &registry, const GraphSnapshot &snapshot) {
    const auto records = storage.getAll();
    if (snapshot.size() != records.size())
        throw std::runtime_error("Graph and inventory device counts disagree.");
    for (const auto &record : records) {
        const auto found = snapshot.find(record.local_id);
        if (found == snapshot.end())
            throw std::runtime_error("Graph and inventory disagree.");
        const auto &details = found->second;
        if (details.device.name != record.name || details.room_id.empty() ||
            !graph.getRoom(details.room_id))
            throw std::runtime_error("Graph and inventory disagree.");
        std::vector<std::string> expected;
        auto *driver = registry.find(record.local_id);
        if (!driver)
            throw std::runtime_error("Inventory driver missing.");
        for (const auto &capability : driver->descriptor().getCapabilities())
            expected.push_back(capability.getId());
        std::sort(expected.begin(), expected.end());
        if (details.ability_ids != expected)
            throw std::runtime_error(
                "Graph capabilities disagree with driver.");
    }
}

std::string display(const Value &value) {
    return std::visit(
        [](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            std::ostringstream output;
            if constexpr (std::is_same_v<T, Color>)
                output << static_cast<int>(item.red) << ' '
                       << static_cast<int>(item.green) << ' '
                       << static_cast<int>(item.blue);
            else if constexpr (std::is_same_v<T, bool>)
                output << (item ? "true" : "false");
            else
                output << item;
            return output.str();
        },
        value);
}

std::vector<Value> commandArguments(const std::string &operation,
                                    const std::vector<std::string> &words,
                                    std::size_t start) {
    std::vector<Value> result;
    if (operation == "on_off.set" && words.size() == start + 1) {
        if (words[start] != "true" && words[start] != "false")
            throw std::invalid_argument("Boolean must be true or false.");
        result.emplace_back(words[start] == "true");
    } else if (operation == "brightness.set" && words.size() == start + 1)
        result.emplace_back(integer<int>(words[start]));
    else if (operation == "color.set" && words.size() == start + 3) {
        std::array<std::uint8_t, 3> rgb{};
        for (std::size_t i = 0; i < 3; ++i) {
            const int component = integer<int>(words[start + i]);
            if (component < 0 || component > 255)
                throw std::invalid_argument("RGB component must be 0..255.");
            rgb[i] = static_cast<std::uint8_t>(component);
        }
        result.emplace_back(Color{rgb[0], rgb[1], rgb[2]});
    } else if (words.size() != start || operation.size() <= 4 ||
               operation.compare(operation.size() - 4, 4, ".get") != 0 ||
               !CapabilityCatalog::find(std::string_view(operation).substr(0, operation.size() - 4)))
        throw std::invalid_argument(
            "Unknown operation or wrong argument count.");
    return result;
}

int run(const persist::PersistenceConfig &config) {
    platform::EventPipe signal_events;
    platform::SignalHandler signals(signal_events);
    persist::StateLock state_lock(config.state_dir);
    const auto saved =
        persist::GenerationManager(state_lock).resolveCommitted();
    persist::requireRamDirectory(config.ram_dir);
    platform::TemporaryDirectory workspace(config.ram_dir,
                                           "smart-home-runtime-");
    const auto inventory_path = workspace.path() / "state.sqlite";
    const auto graph_path = workspace.path() / "ontology.lbdb";
    if (saved) {
        fs::copy_file(saved->path / "state.sqlite", inventory_path);
        fs::copy_file(saved->path / "ontology.lbdb", graph_path);
    }
    SQLiteDeviceStorage storage(inventory_path.string());
    persist::DeviceStateStore database(inventory_path);
    const auto restored = saved ? database.loadProperties(saved->revision)
                                : DevicePropertiesMap{};
    if (!saved)
        database.initializeSchema();
    const bool migrated = storage.replaceDriverIds(
        {"simulated.light", "simulated.temperature_sensor"}, "simulated") != 0;
    DriverFactoryRegistry factories;
    factories.add("simulated", createSimulatedDriver);
    ontology::OntologyConfig graph_config;
    graph_config.database_path = graph_path;
    graph_config.ram_workspace_base = workspace.path();
    graph_config.allow_disk_fallback = false;
    Ontology graph(graph_config);
    // Preserve committed graph contents for validation before accepting
    // startup.
    GraphSnapshot saved_graph;
    if (saved)
        saved_graph = snapshotGraph(graph);
    bool initialized = prepareGraph(graph);
    DeviceRegistry registry;
    DeviceManager manager(storage, factories, registry, graph);
    manager.restoreStoredDevices();
    CommandDispatcher dispatcher(registry);
    if (saved) {
        validateGraph(graph, storage, registry, saved_graph);
        restoreProperties(storage, registry, restored);
    }
    initialized = assignDefaultRooms(graph, storage, registry) || initialized || migrated;
    std::vector<std::unique_ptr<IDeviceDiscoverySource>> sources;
    sources.push_back(std::make_unique<SimulatedDiscoverySource>(
        std::vector<DiscoveredDevice>{
            {"SIM-LIGHT-001", "simulated", "Light1", {"on_off", "brightness", "color"}},
            {"SIM-LIGHT-002", "simulated", "Light2", {"on_off", "brightness", "color"}},
            {"SIM-TEMP-001", "simulated", "TemperatureSensor in bath", {"temperature"}}},
        false));
    DiscoveryService discovery(manager, std::move(sources));
    runtime::ConsoleInput console(STDIN_FILENO);
    persist::SaveWorker worker(state_lock, config.capacity);
    ev::EventBus bus;
    std::exception_ptr reactor_error;
    ev::Reactor reactor([&](std::exception_ptr error) {
        reactor_error = std::move(error);
        reactor.stop();
    });
    const auto checkReactor = [&] {
        if (reactor_error)
            std::rethrow_exception(reactor_error);
    };
    // All startup resources are ready. Keep live RAM state if runtime
    // completion fails.
    workspace.preserve();
    Revision revision = saved ? saved->revision : 0;
    Revision confirmed = revision;
    bool unsaved = initialized;
    bool last_save_success = true;
    const auto markChanged = [&](Revision changed_revision) {
        revision = changed_revision;
        unsaved = true;
    };
    if (initialized)
        markChanged(persist::nextRevision(revision));
    const auto save_subscription = bus.subscribe<persist::SaveResult>(
        [&](const persist::SaveResult &result) {
            last_save_success = result.success;
            if (result.success) {
                confirmed = std::max(confirmed, result.revision);
                if (confirmed == revision)
                    unsaved = false;
                std::cout << "Saved revision " << result.revision << '\n';
            } else {
                if (result.revision == revision)
                    unsaved = true;
                std::cout << "Save failed for revision " << result.revision
                          << "; changes remain in RAM. Use save to retry.\n";
                log::log(log::Level::Error, "Runtime",
                         "Generation commit failed; RAM state retained.");
            }
        });
    const auto collect = [&] {
        for (const auto &result : worker.collectResults())
            bus.publish(result);
        std::cout.flush();
    };
    const auto prepare = [&](persist::Reservation &reservation) {
        try {
            validateGraph(graph, storage, registry, snapshotGraph(graph));
            persist::Snapshot snapshot(workspace.path(), revision);
            database.storeProperties(capture(storage, registry), revision);
            database.backupTo(snapshot.directory() / "state.sqlite");
            graph.dumpToFile((snapshot.directory() / "ontology.lbdb").string());
            reservation.submit(std::move(snapshot));
            std::cout << "Save queued revision " << revision << '\n';
            return true;
        } catch (...) {
            unsaved = true;
            std::cout << "Save preparation failed; changes remain in RAM. Use "
                         "save to retry.\n";
            log::log(log::Level::Error, "Runtime",
                     "Snapshot preparation failed; RAM state retained.");
            return false;
        }
    };
    const auto scan = [&] {
        auto reservation = worker.reserve();
        if (!reservation) {
            std::cout << "Error: snapshot queue full; scan rejected before "
                         "execution.\n";
            return;
        }
        if (revision == std::numeric_limits<Revision>::max()) {
            std::cout << "Error: Runtime revision overflow.\n";
            return;
        }
        const auto changed_revision = persist::nextRevision(revision);
        bool changed = false;
        try {
            const auto before = snapshotGraph(graph);
            const auto count = storage.getAll().size();
            const auto report = discovery.scanOnce();
            std::cout << "Scan: " << report.devices_processed << " processed, "
                      << report.errors.size() << " errors\n";
            changed = assignDefaultRooms(graph, storage, registry) ||
                      storage.getAll().size() != count || snapshotGraph(graph) != before;
        } catch (...) {
            changed = true; // A partial inventory/graph operation must never be
                            // reported as a clean scan.
            std::cout << "Error: scan incomplete; changes remain in RAM. Retry "
                         "scan.\n";
            log::log(log::Level::Error, "Runtime",
                     "Discovery/graph operation incomplete.");
        }
        if (changed)
            markChanged(changed_revision);
        if (changed || initialized) {
            initialized = false;
            prepare(reservation);
        }
    };
    // Console categories are a runtime convention; ontology stores no type
    // field.
    const auto consoleType = [&](DeviceId id) {
        return type(requireDriver(registry, id));
    };
    const auto show = [&](DeviceId id) {
        const auto details = graph.getDeviceDetails(id);
        if (!details)
            throw std::invalid_argument("Unknown device ID.");
        std::cout << "Device " << id << " " << details->device.name << " "
                  << consoleType(id) << " room=" << details->room_id << '\n';
        for (const auto &ability : details->ability_ids) {
            const auto value =
                dispatcher.execute(Command(id, ability + ".get", {}));
            std::cout << "  " << ability << " = " << display(value.value())
                      << '\n';
        }
    };
    bool exiting = false;
    const auto stop_subscription = bus.subscribe<StopRequestedEvent>(
        [&](const StopRequestedEvent &) {
            exiting = true;
            reactor.stop();
        });
    const auto command = [&](const std::string &line) {
        std::istringstream input(line);
        std::vector<std::string> words;
        for (std::string word; input >> word;)
            words.push_back(std::move(word));
        if (words.empty())
            return;
        try {
            if (words[0] == "help" && words.size() == 1) {
                std::cout << "help | list | show <id> | scan | save | exit\n"
                             "<id> <operation> [arguments]\n<room> <type> "
                             "<operation> [arguments]\n"
                             "Rooms: living_room, bathroom; types: light, "
                             "temperature_sensor\n"
                             "Operations: on_off.get/set (true|false), "
                             "brightness.get/set (0..100), color.get/set (R G "
                             "B, 0..255), temperature.get\n";
                return;
            }
            if (words[0] == "exit" && words.size() == 1) {
                bus.publish(StopRequestedEvent{});
                return;
            }
            if (words[0] == "list" && words.size() == 1) {
                for (const auto &device : graph.getAllDevices()) {
                    const auto room = graph.getDeviceRoom(device.id);
                    std::cout << "Device " << device.id << " " << device.name
                              << " " << consoleType(device.id)
                              << " room=" << (room ? room->id : "") << '\n';
                }
                return;
            }
            if (words[0] == "show" && words.size() == 2) {
                const auto id = integer<DeviceId>(words[1]);
                if (id < 0)
                    throw std::invalid_argument(
                        "Device ID must be nonnegative.");
                show(id);
                return;
            }
            if (words[0] == "scan" && words.size() == 1) {
                scan();
                return;
            }
            if (words[0] == "save" && words.size() == 1) {
                auto reservation = worker.reserve();
                if (!reservation)
                    throw std::runtime_error(
                        "Snapshot queue full; save was not queued.");
                prepare(reservation);
                return;
            }
            std::vector<DeviceId> targets;
            std::size_t operation_index;
            const bool address = !words[0].empty() &&
                                 (words[0][0] == '-' ||
                                  (words[0][0] >= '0' && words[0][0] <= '9'));
            if (address) {
                const auto id = integer<DeviceId>(words[0]);
                if (id < 0)
                    throw std::invalid_argument(
                        "Device ID must be nonnegative.");
                targets.push_back(id);
                operation_index = 1;
            } else {
                if (words.size() < 3)
                    throw std::invalid_argument(
                        "Expected room, type and operation.");
                if (words[1] != "light" && words[1] != "temperature_sensor")
                    throw std::invalid_argument("Unknown device type.");
                for (const auto &device : graph.getDevicesInRoom(words[0]))
                    if (consoleType(device.id) == words[1])
                        targets.push_back(device.id);
                if (targets.empty())
                    throw std::invalid_argument("No matching devices.");
                operation_index = 2;
            }
            if (words.size() <= operation_index)
                throw std::invalid_argument("Missing operation.");
            const auto &operation = words[operation_index];
            const auto values =
                commandArguments(operation, words, operation_index + 1);
            const bool mutating =
                operation.size() >= 4 &&
                operation.substr(operation.size() - 4) == ".set";
            // Even a no-op setter reserves before execution, then releases the
            // unused slot.
            std::optional<persist::Reservation> reservation;
            DevicePropertiesMap before;
            Revision changed_revision = revision;
            if (mutating) {
                reservation.emplace(worker.reserve());
                if (!*reservation)
                    throw std::runtime_error("Snapshot queue full; change "
                                             "rejected before execution.");
                changed_revision = persist::nextRevision(revision);
                before = capture(storage, registry);
            }
            std::sort(targets.begin(), targets.end());
            for (const auto id : targets) {
                try {
#if defined(SMART_HOME_RUNTIME_TESTING) && SMART_HOME_RUNTIME_TESTING == 1
                    PERSISTENCE_TEST_POINT(
                        ("target-" + std::to_string(id)).c_str(), revision);
#endif
                    const auto result =
                        dispatcher.execute(Command(id, operation, values));
                    std::cout << "Device " << id << " " << operation << ": "
                              << (result ? display(*result) : "applied")
                              << '\n';
                } catch (const std::exception &error) {
                    std::cout << "Device " << id << " error: " << error.what()
                              << '\n';
                    log::log(log::Level::Warn, "Runtime",
                             "Device command failed.");
                }
            }
            if (mutating) {
                try {
                    if (before == capture(storage, registry))
                        return;
                } catch (...) {
                    // A setter may have succeeded before the comparison failed.
                    markChanged(changed_revision);
                    throw;
                }
                markChanged(changed_revision);
                std::cout << "Changes applied; save pending revision "
                          << revision << '\n';
                prepare(*reservation);
            }
        } catch (const std::exception &error) {
            std::cout << "Error: " << error.what() << '\n';
            log::log(log::Level::Warn, "Runtime", "Console command rejected.");
        }
    };
    const auto command_subscription = bus.subscribe<ConsoleCommandEvent>(
        [&](const ConsoleCommandEvent &event) {
            command(event.line);
            std::cout.flush();
        });

    bool input_queued = false;
    const std::function<void()> read_console = [&] {
        input_queued = false;
        if (exiting || signals.isStopRequested())
            return;
        auto input = console.next();
        if (signals.isStopRequested())
            return;
        using Status = runtime::ConsoleInput::Status;
        if (input.status == Status::Line || (input.status == Status::End && !input.line.empty()))
            bus.publish(ConsoleCommandEvent{std::move(input.line)});
        if (input.status == Status::TooLong)
            std::cout << "Error: input line too long.\n" << std::flush;
        if (input.status == Status::End && !exiting)
            bus.publish(StopRequestedEvent{});
        if (!exiting && console.hasBufferedInput()) {
            reactor.post(read_console);
            input_queued = true;
        }
    };

    reactor.addHandler(signals.eventFd(), ev::IoEvent::Read, [&](int, ev::IoEvent ready) {
        if ((ready & (ev::IoEvent::Error | ev::IoEvent::HangUp)) != ev::IoEvent::None)
            throw std::runtime_error("Signal event pipe unavailable.");
        signals.drain();
        bus.publish(StopRequestedEvent{});
    });
    reactor.addHandler(worker.eventFd(), ev::IoEvent::Read, [&](int, ev::IoEvent ready) {
        if ((ready & (ev::IoEvent::Error | ev::IoEvent::HangUp)) != ev::IoEvent::None)
            throw std::runtime_error("Save event pipe unavailable.");
        collect();
    });
    reactor.addHandler(STDIN_FILENO, ev::IoEvent::Read, [&](int, ev::IoEvent ready) {
        if ((ready & (ev::IoEvent::Read | ev::IoEvent::HangUp)) == ev::IoEvent::None)
            throw std::runtime_error("stdin unavailable.");
        if (!input_queued) {
            reactor.post(read_console);
            input_queued = true;
        }
    });
    reactor.post([&] {
        if (!signals.isStopRequested())
            scan();
        std::cout.flush();
    });

    std::cout << "Runtime ready. Type help.\n" << std::flush;
    reactor.run();
    checkReactor();
    reactor.removeHandler(STDIN_FILENO);
    reactor.removeHandler(signals.eventFd());
    std::cout << "Stopping; final save pending.\n" << std::flush;
    bool final_queued = false;
    for (;;) {
        collect();
        auto reservation = worker.reserve();
        if (reservation) {
            final_queued = prepare(reservation);
            break;
        }
        reactor.step();
        checkReactor();
    }
    worker.stop();
    collect();
    bool closed = true;
    try {
        graph.close();
    } catch (...) {
        closed = false;
        log::log(log::Level::Error, "Runtime", "Final ontology close failed.");
    }
    const bool success = final_queued && last_save_success &&
                         confirmed == revision && !unsaved && closed;
    workspace.setPreserve(!success);
    if (!success)
        std::cerr << "Final save failed; recoverable RAM files retained at "
                  << workspace.path() << '\n';
    return success ? 0 : 1;
}
} // namespace

int main(int argc, char *argv[]) {
    persist::PersistenceConfig config;
    log::Config logging_config;
    std::vector<const char *> logging_arguments{argv[0]};
    try {
        config = arguments(argc, argv, logging_arguments);
    } catch (const std::exception &error) {
        std::cerr << "Runtime configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        logging_config =
            log::resolveConfig(static_cast<int>(logging_arguments.size()),
                               logging_arguments.data(), std::cerr);
    } catch (const std::invalid_argument &error) {
        std::cerr << "Logging configuration error: " << error.what() << '\n';
        return 2;
    }
    try {
        log::Session session(logging_config);
        log::log(log::Level::Info, "Main", "Application started.");
        try {
            const int result = run(config);
            log::log(log::Level::Info, "Main", "Application finished.");
            return result;
        } catch (const std::exception &error) {
            std::cerr << "Runtime failure: " << error.what() << '\n';
            log::log(log::Level::Critical, "Main",
                     "Unhandled application failure; exiting.");
            return 1;
        } catch (...) {
            log::log(log::Level::Critical, "Main",
                     "Unknown application failure; exiting.");
            return 1;
        }
    } catch (...) {
        std::cerr << "Cannot initialize logging configuration.\n";
        return 2;
    }
}
