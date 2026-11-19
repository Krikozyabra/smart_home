#include "DeviceManagementTestSupport.h"
#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "devices/views/BrightnessView.h"
#include "devices/views/OnOffView.h"
#include "drivers/CapabilityCatalog.h"
#include "drivers/UniversalDeviceDriver.h"
#include "storage/InMemoryDeviceStorage.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace smart_home;
using namespace smart_home::ontology;
using device_management_test::GraphFixture;

// Forward to the real graph, counting writes and injecting a single link failure.
class ObservedGraph final : public IOntologyStorage {
    IOntologyStorage &graph;
public:
    int nodes_added = 0, links_added = 0, dump_calls = 0, close_calls = 0;
    int links_before_failure = -1;
    explicit ObservedGraph(IOntologyStorage &value) : graph(value) {}
    void addRoom(const RoomRecord &r) override { graph.addRoom(r); }
    std::optional<RoomRecord> getRoom(const std::string &id) const override { return graph.getRoom(id); }
    std::vector<RoomRecord> getAllRooms() const override { return graph.getAllRooms(); }
    bool deleteRoom(const std::string &id) override { return graph.deleteRoom(id); }
    void addDevice(const OntologyDeviceRecord &r) override { ++nodes_added; graph.addDevice(r); }
    std::optional<OntologyDeviceRecord> getDevice(DeviceId id) const override { return graph.getDevice(id); }
    std::vector<OntologyDeviceRecord> getAllDevices() const override { return graph.getAllDevices(); }
    bool deleteDevice(DeviceId id) override { return graph.deleteDevice(id); }
    void addAbility(const AbilityRecord &r) override { graph.addAbility(r); }
    std::optional<AbilityRecord> getAbility(const std::string &id) const override { return graph.getAbility(id); }
    std::vector<AbilityRecord> getAllAbilities() const override { return graph.getAllAbilities(); }
    bool deleteAbility(const std::string &id) override { return graph.deleteAbility(id); }
    void assignDeviceToRoom(DeviceId, const std::string &) override { throw std::logic_error("Manager must not assign rooms"); }
    void removeDeviceFromRoom(DeviceId) override { throw std::logic_error("Manager must not remove rooms"); }
    std::optional<RoomRecord> getDeviceRoom(DeviceId id) const override { return graph.getDeviceRoom(id); }
    std::vector<OntologyDeviceRecord> getDevicesInRoom(const std::string &id) const override { return graph.getDevicesInRoom(id); }
    void addDeviceAbility(DeviceId id, const std::string &ability) override {
        if (links_before_failure == 0) {
            links_before_failure = -1;
            throw std::runtime_error("Injected link failure");
        }
        if (links_before_failure > 0) --links_before_failure;
        graph.addDeviceAbility(id, ability);
        ++links_added;
    }
    void removeDeviceAbility(DeviceId id, const std::string &ability) override { graph.removeDeviceAbility(id, ability); }
    std::vector<AbilityRecord> getDeviceAbilities(DeviceId id) const override { return graph.getDeviceAbilities(id); }
    std::vector<OntologyDeviceRecord> getDevicesWithAbility(const std::string &id) const override { return graph.getDevicesWithAbility(id); }
    std::optional<DeviceOntologyDetails> getDeviceDetails(DeviceId id) const override { return graph.getDeviceDetails(id); }
    void dumpToFile(const std::string&) override { ++dump_calls; throw std::logic_error("Manager must not dump"); }
    void close() override { ++close_calls; throw std::logic_error("Manager must not close"); }
};

struct FactoryFailure : std::runtime_error { using std::runtime_error::runtime_error; };
class FixedLightDriver final : public DeviceDriverBase {
public:
    explicit FixedLightDriver(const DeviceRecord &record)
        : DeviceDriverBase(std::make_unique<Device>(record.local_id, record.name),
                           {record.local_id, record.physical_id, record.name,
                            {CapabilityCatalog::onOff().descriptor,
                             CapabilityCatalog::brightness().descriptor,
                             CapabilityCatalog::color().descriptor}}) {
        for (const auto &capability : descriptor().getCapabilities()) {
            const auto &entry = *CapabilityCatalog::find(capability.getId());
            entry.store(device(), entry.initial_value);
        }
    }
};

struct ControlledFactory {
    int calls = 0;
    DeviceCreator action = light;
    static std::unique_ptr<IDeviceDriver> light(const DeviceRecord &record) {
        return std::make_unique<FixedLightDriver>(record);
    }
    std::unique_ptr<IDeviceDriver> operator()(const DeviceRecord &record) {
        ++calls;
        return action(record);
    }
};

struct Fixture {
    GraphFixture database;
    ObservedGraph graph{database.graph};
    InMemoryDeviceStorage storage;
    DriverFactoryRegistry factories;
    DeviceRegistry devices;
    ControlledFactory factory;
    Fixture() {
        factories.add("test.driver", std::ref(factory));
    }
    DeviceRecord insert(const std::string &physical = "a") {
        return storage.insert(physical, "test.driver", "Light " + physical);
    }
    void complete(const DeviceRecord &r) {
        const auto details = graph.getDeviceDetails(r.local_id);
        require(details && details->device.id == r.local_id && details->device.name == r.name,
                "Graph node must match inventory");
        require(details->ability_ids == std::vector<std::string>{"brightness", "color", "on_off"},
                "Graph must contain exactly the driver's capabilities");
    }
};

void registrationAndRepair() {
    Fixture f;
    {
        DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
        require(f.storage.getAll().empty() && f.devices.size() == 0 &&
                    f.graph.getAllDevices().empty() && f.factory.calls == 0,
                "Constructor must have no side effects");
        auto record = manager.registerDiscoveredDevice("a", "test.driver", "Saved name");
        require(f.storage.getAll().size() == 1 && f.devices.size() == 1,
                "First registration must create inventory and driver");
        f.complete(record);
        auto *driver = f.devices.find(record.local_id);
        auto brightness = driver->device().view<BrightnessView>();
        auto power = driver->device().view<OnOffView>();
        brightness.setBrightness(83);
        power.setOn(true);
        f.database.graph.addRoom({"custom", "User room"});
        f.database.graph.assignDeviceToRoom(record.local_id, "custom");
        const auto catalog = f.graph.getAllAbilities();
        require(manager.registerDiscoveredDevice("a", "test.driver", "Ignored") == record,
                "Repeated registration must return the stored record");
        require(f.factory.calls == 1 && f.devices.find(record.local_id) == driver &&
                    brightness.getBrightness() == 83 && power.isOn(),
                "Repeated registration must preserve driver and its values");
        require(f.graph.nodes_added == 1 && f.graph.links_added == 3 &&
                    f.graph.getAllAbilities() == catalog,
                "Repeat must not rewrite nodes, links or catalog");
        f.database.graph.removeDeviceAbility(record.local_id, "color");
        manager.registerDiscoveredDevice("a", "test.driver", "Ignored");
        f.complete(record);
        require(manager.restoreStoredDevices() == 0 &&
                    f.graph.getDeviceRoom(record.local_id)->id == "custom" &&
                    brightness.getBrightness() == 83 && power.isOn(),
                "Restore must preserve the room and active state");
        f.database.graph.deleteDevice(record.local_id);
        require(manager.restoreStoredDevices() == 0, "Graph-only restore must add zero drivers");
        f.complete(record);
        require(!f.graph.getDeviceRoom(record.local_id) && f.factory.calls == 1,
                "Manager must not assign rooms or recreate active drivers");
        f.database.graph.deleteDevice(record.local_id);
        manager.registerDiscoveredDevice("a", "test.driver", "Ignored");
        f.complete(record);
    }
    require(f.graph.dump_calls == 0 && f.graph.close_calls == 0,
            "Manager lifetime must not save or close ontology");
}

void restoreAndPrepareFailures() {
    Fixture f;
    DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
    require(manager.restoreStoredDevices() == 0, "Empty restore must return zero");
    const auto a = f.insert("a");
    const auto b = f.insert("b");
    const auto existing = f.insert("existing");
    f.devices.add(ControlledFactory::light(existing));
    auto *existing_driver = f.devices.find(existing.local_id);
    f.database.graph.addDevice({existing.local_id, existing.name});
    // InMemoryDeviceStorage orders these by physical key, so a is prepared first.
    f.factory.action = [&](const DeviceRecord &r) {
        if (r.local_id == b.local_id) throw FactoryFailure("Factory failed");
        return ControlledFactory::light(r);
    };
    expectException<FactoryFailure>([&] { manager.restoreStoredDevices(); });
    require(f.factory.calls == 2 && f.devices.size() == 1 && f.graph.nodes_added == 0 &&
                f.devices.find(existing.local_id) == existing_driver &&
                f.graph.getDeviceAbilities(existing.local_id).empty(),
            "Late preparation failure must leave registry and graph unchanged");
    f.factory.action = ControlledFactory::light;
    require(manager.restoreStoredDevices() == 2, "Restore must count newly added drivers");
    f.complete(a); f.complete(b); f.complete(existing);
    auto *active = f.devices.find(a.local_id);
    require(manager.restoreStoredDevices() == 0 && f.devices.find(a.local_id) == active &&
                f.factory.calls == 4 && f.storage.getAll().size() == 3,
            "Repeated restore must preserve drivers and inventory");
    f.database.graph.removeDeviceAbility(a.local_id, "brightness");
    f.database.graph.deleteDevice(b.local_id);
    DriverFactoryRegistry no_factories;
    DeviceManager active_only(f.storage, no_factories, f.devices, f.graph);
    require(active_only.restoreStoredDevices() == 0,
            "Active devices must not need factories when restoring graph");
    f.complete(a); f.complete(b);
    expectException<UnsupportedDriverError>([&] {
        active_only.registerDiscoveredDevice("a", "test.driver", "Ignored");
    });
}

void unknownFactory() {
    Fixture f;
    DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
    // Empty strings would fail in storage, proving the factory is checked first.
    expectException<UnsupportedDriverError>([&] {
        manager.registerDiscoveredDevice("", "unknown.driver", "");
    });
    require(f.storage.getAll().empty() && f.devices.size() == 0 && f.graph.nodes_added == 0,
            "Unsupported registration must not change any dependency");
    f.insert("a");
    f.storage.insert("z", "unknown.driver", "Unknown");
    expectException<std::runtime_error>([&] { manager.restoreStoredDevices(); });
    require(f.factory.calls == 1 && f.devices.size() == 0 && f.graph.nodes_added == 0,
            "Unknown late factory must cancel all prepared drivers");
}

void nullAndThrowingFactory() {
    for (bool null_result : {false, true}) {
        Fixture f;
        DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
        f.factory.action = [=](const DeviceRecord &) -> std::unique_ptr<IDeviceDriver> {
            if (null_result) return nullptr;
            throw FactoryFailure("Factory failed after insert");
        };
        const auto register_device = [&] { manager.registerDiscoveredDevice("a", "test.driver", "Saved"); };
        if (null_result) expectException<std::logic_error>(register_device);
        else expectException<FactoryFailure>(register_device);
        require(f.storage.getAll().size() == 1 && f.devices.size() == 0 && f.graph.nodes_added == 0,
                "Failed factory must leave the inventory record available for retry");
        f.factory.action = ControlledFactory::light;
        auto record = manager.registerDiscoveredDevice("a", "test.driver", "Ignored");
        require(record.name == "Saved" && f.storage.getAll().size() == 1,
                "Factory retry must reuse the saved record");
        f.complete(record);
    }
    Fixture f;
    f.insert("a"); f.insert("b");
    f.factory.action = [](const DeviceRecord &r) {
        return r.physical_id == "b" ? nullptr : ControlledFactory::light(r);
    };
    DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
    expectException<std::logic_error>([&] { manager.restoreStoredDevices(); });
    require(f.devices.size() == 0 && f.graph.nodes_added == 0,
            "Null late factory must preserve preparation atomicity");
}

void strictGraphChecks() {
    for (const std::string problem : {"catalog", "name", "extra"}) {
        for (bool restore : {false, true}) {
            Fixture f;
            const auto record = f.insert();
            if (problem == "catalog") f.database.graph.deleteAbility("brightness");
            else {
                f.database.graph.addDevice({record.local_id, problem == "name" ? "Wrong" : record.name});
                if (problem == "extra") f.database.graph.addDeviceAbility(record.local_id, "temperature");
            }
            DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
            const auto attempt = [&] {
                if (restore) manager.restoreStoredDevices();
                else manager.registerDiscoveredDevice("a", "test.driver", "Ignored");
            };
            expectException<std::runtime_error>(attempt);
            require(f.devices.size() == 1 && f.factory.calls == 1,
                    "Graph validation failure must retain the active driver");
            if (problem == "name") {
                require(f.graph.getDevice(record.local_id)->name == "Wrong", "Manager must not overwrite mismatched names");
                f.database.graph.addDevice({record.local_id, record.name});
            } else if (problem == "extra") {
                require(f.graph.getDeviceAbilities(record.local_id).size() == 4, "Manager must not delete extra links");
                f.database.graph.removeDeviceAbility(record.local_id, "temperature");
            } else {
                require(!f.graph.getAbility("brightness"), "Manager must not create catalog entries");
                f.database.graph.addAbility({"brightness", "Custom brightness", "custom"});
            }
            attempt();
            f.complete(record);
            require(f.factory.calls == 1, "Graph repair must preserve the existing driver");
        }
    }
}

void partialGraphFailure() {
    for (bool restore : {false, true}) {
        Fixture f;
        if (restore) { f.insert("a"); f.insert("b"); }
        f.graph.links_before_failure = restore ? 4 : 1;
        DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
        const auto attempt = [&] {
            if (restore) return manager.restoreStoredDevices();
            manager.registerDiscoveredDevice("a", "test.driver", "Saved");
            return std::size_t{0};
        };
        expectException<std::runtime_error>(attempt);
        const auto records = f.storage.getAll();
        const int created = f.factory.calls;
        auto *driver = f.devices.find(records.front().local_id);
        require(f.devices.size() == records.size() && f.graph.links_added == (restore ? 4 : 1),
                "Failure must retain all drivers and successful graph writes");
        require(attempt() == 0, "Retry adds no drivers, even after incomplete graph restore");
        for (const auto &r : records) f.complete(r);
        require(f.factory.calls == created && f.devices.find(records.front().local_id) == driver &&
                    f.graph.nodes_added == static_cast<int>(records.size()) &&
                    f.graph.links_added == static_cast<int>(records.size() * 3),
                "Retry must complete only missing nodes and links");
    }
}

void registryFailureAndRetry() {
    Fixture f;
    const auto a = f.insert("a");
    const auto b = f.insert("b");
    // The manager delegates driver identity to the factory; this faulty factory
    // creates a duplicate registry key after the first driver was added.
    f.factory.action = [&](const DeviceRecord &r) {
        auto wrong = r;
        wrong.local_id = a.local_id;
        return ControlledFactory::light(wrong);
    };
    DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
    expectException<std::runtime_error>([&] { manager.restoreStoredDevices(); });
    auto *active = f.devices.find(a.local_id);
    require(active && f.devices.size() == 1 && f.graph.nodes_added == 0,
            "Registry failure must retain earlier additions and not start graph writes");
    expectException<std::runtime_error>([&] {
        manager.registerDiscoveredDevice("b", "test.driver", "Ignored");
    });
    require(f.storage.getAll().size() == 2 && f.devices.size() == 1 &&
                f.graph.nodes_added == 0,
            "Registration failure must preserve inventory without writing the graph");
    f.factory.action = ControlledFactory::light;
    require(manager.restoreStoredDevices() == 1 && f.devices.find(a.local_id) == active,
            "Retry after registry failure must create only the missing driver");
    f.complete(a); f.complete(b);
}

class CustomDriver final : public DeviceDriverBase {
public:
    explicit CustomDriver(const DeviceRecord &r)
        : DeviceDriverBase(std::make_unique<Device>(r.local_id, r.name),
                           DeviceDescriptor(r.local_id, r.physical_id, r.name,
                                            {CapabilityDescriptor("custom.feature", "Custom", {})})) {}
};

void customCapability() {
    Fixture f;
    f.database.graph.addAbility({"custom.feature", "Custom feature", "custom"});
    f.factory.action = [](const DeviceRecord &r) { return std::make_unique<CustomDriver>(r); };
    DeviceManager manager(f.storage, f.factories, f.devices, f.graph);
    const auto record = manager.registerDiscoveredDevice("custom", "test.driver", "Custom device");
    require(f.graph.getDeviceDetails(record.local_id)->ability_ids ==
                std::vector<std::string>{"custom.feature"},
            "Capabilities must come from the descriptor without driver or type mappings");
}

void protocolRestoration() {
    GraphFixture fixture;
    InMemoryDeviceStorage storage;
    DriverFactoryRegistry creators;
    creators.add("simulated", createSimulatedDriver);
    DeviceRegistry active;
    DeviceManager manager(storage, creators, active, fixture.graph);
    expectException<std::invalid_argument>([&] {
        manager.registerDiscoveredDevice("mixed", "simulated", "Mixed", {"unknown"});
    });
    require(storage.getAll().empty(), "Invalid discovery metadata must not insert a record");

    const auto record = manager.registerDiscoveredDevice(
        "mixed", "simulated", "Mixed", {"on_off", "temperature"});
    auto *driver = active.find(record.local_id);
    driver->writeCapability("on_off", true);
    require(manager.registerDiscoveredDevice("mixed", "simulated", "Ignored",
                                              {"on_off", "battery"}) == record,
            "Rediscovery must preserve inventory identity");
    require(active.find(record.local_id) == driver &&
                driver->device().getProperty("on_off.state") == Value{true} &&
                fixture.graph.getDeviceDetails(record.local_id)->ability_ids ==
                    std::vector<std::string>{"battery", "on_off", "temperature"},
            "Rediscovery must add capabilities without losing active state");

    DeviceRegistry cold;
    DeviceManager restarted(storage, creators, cold, fixture.graph);
    require(restarted.restoreStoredDevices() == 1 && restarted.restoreStoredDevices() == 0,
            "Cold restoration must create each protocol driver once");
    const auto &restored = cold.find(record.local_id)->device();
    require(restored.getProperty("on_off.state") == Value{false} &&
                restored.getProperty("temperature.celsius") == Value{23.0} &&
                restored.getProperty("battery.level") == Value{0},
            "Cold restoration must initialize all graph capabilities");
}
} // namespace

int main() {
    registrationAndRepair();
    restoreAndPrepareFailures();
    unknownFactory();
    nullAndThrowingFactory();
    strictGraphChecks();
    partialGraphFailure();
    registryFailureAndRetry();
    customCapability();
    protocolRestoration();
}
