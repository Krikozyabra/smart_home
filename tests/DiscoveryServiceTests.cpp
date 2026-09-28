#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "discovery/DiscoveryService.h"
#include "discovery/SimulatedDiscoverySource.h"
#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/InMemoryDeviceStorage.h"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

int main() {
    {
        smart_home::InMemoryDeviceStorage persist_storage;
        smart_home::DriverFactoryRegistry factory_registry;
        smart_home::DeviceRegistry runtime_registry;
        smart_home::DeviceManager device_manager(
            persist_storage, factory_registry, runtime_registry);

        std::vector<std::unique_ptr<smart_home::IDeviceDiscoverySource>>
            sources_list;
        sources_list.push_back(
            std::make_unique<smart_home::SimulatedDiscoverySource>(
                std::vector<smart_home::DiscoveredDevice>{})); // put empty
                                                               // source in
                                                               // sources list

        smart_home::DiscoveryService ds(device_manager,
                                        std::move(sources_list));
        ds.scanOnce();
        require(persist_storage.getAll().empty(),
                "Empty sources must not change the persist storage");
        require(runtime_registry.size() == 0,
                "Empty sources must not change the runtime registry");
    }
    {
        smart_home::InMemoryDeviceStorage persist_storage;
        smart_home::DriverFactoryRegistry factory_registry;
        smart_home::DeviceRegistry runtime_registry;
        smart_home::DeviceManager device_manager(
            persist_storage, factory_registry, runtime_registry);

        std::vector<std::unique_ptr<smart_home::IDeviceDiscoverySource>>
            sources_list;

        smart_home::DiscoveryService ds(device_manager,
                                        std::move(sources_list));
        ds.scanOnce();
        require(persist_storage.getAll().empty(),
                "Empty list of sources must not change the persist storage");
        require(runtime_registry.size() == 0,
                "Empty list of sources must not change the runtime registry");
    }
    {
        smart_home::InMemoryDeviceStorage persist_storage;
        smart_home::DriverFactoryRegistry factory_registry;
        factory_registry.add(
            std::make_unique<smart_home::SimulatedLightDriverFactory>());
        smart_home::DeviceRegistry runtime_registry;
        smart_home::DeviceManager device_manager(
            persist_storage, factory_registry, runtime_registry);

        std::vector<std::unique_ptr<smart_home::IDeviceDiscoverySource>>
            sources_list;
        sources_list.push_back(
            std::make_unique<smart_home::SimulatedDiscoverySource>(
                std::vector<smart_home::DiscoveredDevice>{
                    smart_home::DiscoveredDevice{"SIM-LIGHT-001",
                                                 "simulated.light", "Light1"},
                    smart_home::DiscoveredDevice{
                        "SIM-LIGHT-002", "simulated.light", "Light2"}}));
        sources_list.push_back(
            std::make_unique<smart_home::SimulatedDiscoverySource>(
                std::vector<smart_home::DiscoveredDevice>{
                    smart_home::DiscoveredDevice{
                        "SIM-LIGHT-001", "simulated.light", "Light3"}}));

        smart_home::DiscoveryService ds(device_manager,
                                        std::move(sources_list));
        ds.scanOnce();
        require(runtime_registry.size() == 2,
                "After scan not empty sources the runtime registry must change "
                "state correctly");
        require(persist_storage.getAll().size() == 2,
                "After scan not empty sources the persist storage must change "
                "state correctly");
        auto sim_light_1 = persist_storage.findByPhysicalId("SIM-LIGHT-001",
                                                            "simulated.light");
        require(sim_light_1.has_value(), "First light should be found");
        auto sim_light_2 = persist_storage.findByPhysicalId("SIM-LIGHT-002",
                                                            "simulated.light");
        require(sim_light_2.has_value(), "Second light should be found");
        ds.scanOnce();
        require(
            runtime_registry.size() == 2,
            "After repeated scan the runtime registry must not change state");
        require(
            persist_storage.getAll().size() == 2,
            "After repeated scan the persist storage must not change state");
        auto sim_light_1_repeated = persist_storage.findByPhysicalId(
            "SIM-LIGHT-001", "simulated.light");
        require(sim_light_1_repeated.has_value(),
                "First light should be found after second scan");
        auto sim_light_2_repeated = persist_storage.findByPhysicalId(
            "SIM-LIGHT-002", "simulated.light");
        require(sim_light_2_repeated.has_value(),
                "Second light should be found after second scan");
        require(sim_light_1 == sim_light_1_repeated,
                "After repeated scan the DiscoveredDevice must not change 1");
        require(sim_light_2 == sim_light_2_repeated,
                "After repeated scan the DiscoveredDevice must not change 2");
    }
    {
        smart_home::InMemoryDeviceStorage persist_storage;
        smart_home::DriverFactoryRegistry factory_registry;
        smart_home::DeviceRegistry runtime_registry;
        smart_home::DeviceManager device_manager(
            persist_storage, factory_registry, runtime_registry);

        std::vector<std::unique_ptr<smart_home::IDeviceDiscoverySource>>
            sources_list;
        sources_list.push_back(nullptr);

        expectException<std::invalid_argument>([&] {
            smart_home::DiscoveryService(device_manager,
                                         std::move(sources_list));
        });
    }
}
