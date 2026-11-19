/** @file main.cpp
 * @brief Композиция приложения, консольный цикл и фиксация полных поколений.
 */
#include "command/Command.h"
#include "device_management/DeviceManager.h"
#include "discovery/DiscoveryService.h"
#include "discovery/SimulatedDiscoverySource.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "execution/CommandDispatcher.h"
#include "logging/Logging.h"
#include "ontology/LadybugOntologyStorage.h"
#include "persistence/DeviceStateStore.h"
#include "persistence/GenerationManager.h"
#include "persistence/PersistenceConfig.h"
#include "persistence/SaveWorker.h"
#include "persistence/StateLock.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/SQLiteDeviceStorage.h"

#if defined(SMART_HOME_RUNTIME_TESTING) && SMART_HOME_RUNTIME_TESTING == 1
#include "Testing.h"
#endif

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace smart_home;
namespace fs = std::filesystem;
namespace log = smart_home::logging;
namespace persist = smart_home::persistence;
using Ontology = ontology::LadybugOntologyStorage;
using persist::DevicePropertiesMap;
using persist::Revision;

[[noreturn]] void systemError(const char *operation) {
    throw std::runtime_error(std::string(operation) + ": " +
                             std::strerror(errno));
}

class Fd {
    int value_ = -1;

  public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() {
        if (value_ >= 0)
            ::close(value_);
    }
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
    int get() const { return value_; }
};

class SignalPipe {
    Fd reader_, writer_;
    static std::array<int, 2> openPipe() {
        std::array<int, 2> fds{};
        if (::pipe2(fds.data(), O_NONBLOCK | O_CLOEXEC) != 0)
            systemError("pipe2");
        return fds;
    }
    explicit SignalPipe(std::array<int, 2> fds)
        : reader_(fds[0]), writer_(fds[1]) {}

  public:
    SignalPipe() : SignalPipe(openPipe()) {}
    int reader() const { return reader_.get(); }
    int writer() const { return writer_.get(); }
    void drain() const {
        char bytes[128];
        while (::read(reader(), bytes, sizeof(bytes)) > 0) {
        }
    }
};

static_assert(std::atomic<bool>::is_always_lock_free &&
                  std::atomic<int>::is_always_lock_free,
              "Signal notification requires lock-free atomics.");
std::atomic<bool> stop_requested{false};
std::atomic<int> signal_writer{-1};
extern "C" void requestStop(int) {
    const int saved_errno = errno;
    stop_requested = 1;
    const char byte = 1;
    const int writer = signal_writer.load(std::memory_order_relaxed);
    if (writer >= 0) {
        const auto ignored = ::write(writer, &byte, 1);
        (void)ignored;
    }
    errno = saved_errno;
}

class Signals {
    struct sigaction old_int_{}, old_term_{};

  public:
    explicit Signals(const SignalPipe &pipe) {
        stop_requested = 0;
        signal_writer = pipe.writer();
        struct sigaction action{};
        action.sa_handler = requestStop;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGINT, &action, &old_int_) != 0) {
            signal_writer = -1;
            systemError("sigaction");
        }
        if (sigaction(SIGTERM, &action, &old_term_) != 0) {
            sigaction(SIGINT, &old_int_, nullptr);
            signal_writer = -1;
            systemError("sigaction");
        }
    }
    ~Signals() {
        sigaction(SIGINT, &old_int_, nullptr);
        sigaction(SIGTERM, &old_term_, nullptr);
        signal_writer = -1;
    }
};

class StdinFlags {
    int old_;

  public:
    StdinFlags() : old_(fcntl(STDIN_FILENO, F_GETFL)) {
        if (old_ < 0 || fcntl(STDIN_FILENO, F_SETFL, old_ | O_NONBLOCK) < 0)
            systemError("stdin flags");
    }
    ~StdinFlags() { fcntl(STDIN_FILENO, F_SETFL, old_); }
};

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

// mkdtemp owns private files even when construction of a later service fails.
class Directory {
  public:
    fs::path path;
    bool preserve = false;
    Directory(const fs::path &base, const std::string &prefix) {
        auto pattern = (base / (prefix + "XXXXXX")).string();
        if (!::mkdtemp(pattern.data()))
            systemError("mkdtemp");
        path = pattern;
    }
    ~Directory() {
        if (!preserve) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }
    Directory(const Directory &) = delete;
    Directory &operator=(const Directory &) = delete;
};

std::string type(const DeviceRecord &record) {
    if (record.driver_id == "simulated.light")
        return "light";
    if (record.driver_id == "simulated.temperature_sensor")
        return "temperature_sensor";
    throw std::runtime_error("Unsupported saved driver type.");
}

template <class T>
const T &property(const DevicePropertiesMap::mapped_type &properties,
                  const char *key) {
    const auto found = properties.find(key);
    const auto *value =
        found == properties.end() ? nullptr : std::get_if<T>(&found->second);
    if (!value)
        throw std::runtime_error("Invalid or missing saved device property.");
    return *value;
}

void validateProperties(const std::vector<DeviceRecord> &records,
                        const DevicePropertiesMap &restored) {
    if (restored.size() != records.size())
        throw std::runtime_error("Incomplete saved device values.");
    for (const auto &record : records) {
        const auto found = restored.find(record.local_id);
        if (found == restored.end())
            throw std::runtime_error("Inventory and values disagree.");
        const auto &values = found->second;
        if (type(record) == "light") {
            const auto brightness =
                property<std::int64_t>(values, "brightness");
            if (values.size() != 3 || brightness < 0 || brightness > 100)
                throw std::runtime_error("Invalid saved light value.");
            property<bool>(values, "on_off");
            property<Color>(values, "color");
        } else {
            if (values.size() != 1)
                throw std::runtime_error(
                    "Invalid saved temperature sensor values.");
            property<double>(values, "temperature");
        }
    }
}

class TemperatureFactory final : public IDeviceDriverFactory {
    const DevicePropertiesMap &restored_;

  public:
    explicit TemperatureFactory(const DevicePropertiesMap &restored)
        : restored_(restored) {}
    std::string getDriverId() const override {
        return "simulated.temperature_sensor";
    }
    std::unique_ptr<IDeviceDriver>
    create(const DeviceRecord &record) const override {
        const auto found = restored_.find(record.local_id);
        return std::make_unique<SimulatedTemperatureSensorDriver>(
            record.local_id, record.physical_id, record.name,
            found == restored_.end()
                ? 23.0
                : property<double>(found->second, "temperature"));
    }
};

DevicePropertiesMap capture(SQLiteDeviceStorage &storage,
                            CommandDispatcher &dispatcher) {
    DevicePropertiesMap result;
    for (const auto &record : storage.getAll()) {
        const auto get = [&](const char *operation) {
            return dispatcher.execute(Command(record.local_id, operation, {}))
                .value();
        };
        auto &values = result[record.local_id];
        if (type(record) == "light") {
            values.emplace("on_off", std::get<bool>(get("on_off.get")));
            values.emplace("brightness",
                           std::int64_t(std::get<int>(get("brightness.get"))));
            values.emplace("color", std::get<Color>(get("color.get")));
        } else
            values.emplace("temperature",
                           std::get<double>(get("temperature.get")));
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
          ontology::AbilityRecord{"temperature", "Температура", "climate"}})
        if (!graph.getAbility(ability.id)) {
            graph.addAbility(ability);
            changed = true;
        }
    return changed;
}

bool assignDefaultRooms(Ontology &graph, SQLiteDeviceStorage &storage) {
    bool changed = false;
    for (const auto &record : storage.getAll())
        if (!graph.getDeviceRoom(record.local_id)) {
            graph.assignDeviceToRoom(record.local_id, type(record) == "light"
                                                          ? "living_room"
                                                          : "bathroom");
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
    } else if ((operation == "on_off.get" || operation == "brightness.get" ||
                operation == "color.get" || operation == "temperature.get") &&
               words.size() == start) {
    } else
        throw std::invalid_argument(
            "Unknown operation or wrong argument count.");
    return result;
}

int run(const persist::PersistenceConfig &config) {
    SignalPipe signal_events;
    Signals signals(signal_events);
    persist::StateLock state_lock(config.state_dir);
    const auto saved =
        persist::GenerationManager(state_lock).resolveCommitted();
    persist::requireRamDirectory(config.ram_dir);
    Directory workspace(config.ram_dir, "smart-home-runtime-");
    const auto inventory_path = workspace.path / "state.sqlite";
    const auto graph_path = workspace.path / "ontology.lbdb";
    if (saved) {
        fs::copy_file(saved->path / "state.sqlite", inventory_path);
        fs::copy_file(saved->path / "ontology.lbdb", graph_path);
    }
    SQLiteDeviceStorage storage(inventory_path.string());
    persist::DeviceStateStore database(inventory_path);
    const auto restored = saved ? database.loadProperties(saved->revision)
                                : DevicePropertiesMap{};
    if (saved)
        validateProperties(storage.getAll(), restored);
    else
        database.initializeSchema();
    DriverFactoryRegistry factories;
    factories.add(std::make_unique<SimulatedLightDriverFactory>());
    factories.add(std::make_unique<TemperatureFactory>(restored));
    ontology::OntologyConfig graph_config;
    graph_config.database_path = graph_path;
    graph_config.ram_workspace_base = workspace.path;
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
    for (const auto &[id, values] : restored)
        if (values.count("on_off")) {
            dispatcher.execute(
                Command(id, "on_off.set", {property<bool>(values, "on_off")}));
            dispatcher.execute(Command(id, "brightness.set",
                                       {static_cast<int>(property<std::int64_t>(
                                           values, "brightness"))}));
            dispatcher.execute(
                Command(id, "color.set", {property<Color>(values, "color")}));
        }
    if (saved)
        validateGraph(graph, storage, registry, saved_graph);
    initialized = assignDefaultRooms(graph, storage) || initialized;
    std::vector<std::unique_ptr<IDeviceDiscoverySource>> sources;
    sources.push_back(std::make_unique<SimulatedDiscoverySource>(
        std::vector<DiscoveredDevice>{
            {"SIM-LIGHT-001", "simulated.light", "Light1"},
            {"SIM-LIGHT-002", "simulated.light", "Light2"},
            {"SIM-TEMP-001", "simulated.temperature_sensor",
             "TemperatureSensor in bath"}},
        false));
    DiscoveryService discovery(manager, std::move(sources));
    StdinFlags stdin_flags;
    persist::SaveWorker worker(state_lock, config.capacity);
    // All startup resources are ready. Keep live RAM state if runtime
    // completion fails.
    workspace.preserve = true;
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
    const auto collect = [&] {
        for (const auto &result : worker.collectResults()) {
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
        }
    };
    const auto prepare = [&](persist::Reservation &reservation) {
        try {
            validateGraph(graph, storage, registry, snapshotGraph(graph));
            persist::Snapshot snapshot(workspace.path, revision);
            database.storeProperties(capture(storage, dispatcher), revision);
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
        const auto count = storage.getAll().size();
        try {
            const auto report = discovery.scanOnce();
            std::cout << "Scan: " << report.devices_processed << " processed, "
                      << report.errors.size() << " errors\n";
            changed = storage.getAll().size() != count;
            changed = assignDefaultRooms(graph, storage) || changed;
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
        for (const auto &record : storage.getAll())
            if (record.local_id == id)
                return type(record);
        throw std::runtime_error("Graph device is absent from inventory.");
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
                exiting = true;
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
                before = capture(storage, dispatcher);
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
                    if (before == capture(storage, dispatcher))
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
    std::cout << "Runtime ready. Type help.\n" << std::flush;
    bool first_iteration = true;
    std::string pending;
    bool eof = false, discard = false;
    while (!exiting && !stop_requested) {
        collect();
        if (first_iteration) {
            first_iteration = false;
            scan();
            continue;
        }
        const auto newline = pending.find('\n');
        if (newline != std::string::npos) {
            const auto line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (line.size() > 65536)
                std::cout << "Error: input line too long.\n";
            else
                command(line);
            std::cout.flush();
            continue;
        }
        if (eof) {
            if (!pending.empty())
                command(pending);
            break;
        }
        std::cout.flush();
        pollfd fds[] = {{STDIN_FILENO, POLLIN, 0},
                        {worker.eventFd(), POLLIN, 0},
                        {signal_events.reader(), POLLIN, 0}};
        const int result = ::poll(fds, 3, -1);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            systemError("poll");
        }
        if (fds[2].revents)
            signal_events.drain();
        if (fds[0].revents & POLLNVAL)
            throw std::runtime_error("stdin unavailable.");
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char bytes[4096];
            const auto size = ::read(STDIN_FILENO, bytes, sizeof(bytes));
            if (size == 0)
                eof = true;
            else if (size < 0) {
                if (errno != EAGAIN && errno != EINTR)
                    systemError("read stdin");
            } else
                for (ssize_t i = 0; i < size; ++i) {
                    if (discard) {
                        if (bytes[i] == '\n')
                            discard = false;
                        continue;
                    }
                    pending.push_back(bytes[i]);
                    if (pending.size() > 65536 &&
                        pending.find('\n') == std::string::npos) {
                        pending.clear();
                        discard = true;
                        std::cout << "Error: input line too long.\n";
                    }
                }
        }
    }
    std::cout << "Stopping; final save pending.\n" << std::flush;
    bool final_queued = false;
    for (;;) {
        collect();
        auto reservation = worker.reserve();
        if (reservation) {
            final_queued = prepare(reservation);
            break;
        }
        pollfd event{worker.eventFd(), POLLIN, 0};
        if (::poll(&event, 1, -1) < 0 && errno != EINTR)
            systemError("poll final save");
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
    workspace.preserve = !success;
    if (!success)
        std::cerr << "Final save failed; recoverable RAM files retained at "
                  << workspace.path << '\n';
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
