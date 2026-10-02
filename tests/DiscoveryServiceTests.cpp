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
#include <optional>
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
                std::vector<smart_home::DiscoveredDevice>{},
                false)); // put empty
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
        auto report = ds.scanOnce();
        require(persist_storage.getAll().empty(),
                "Empty list of sources must not change the persist storage");
        require(runtime_registry.size() == 0,
                "Empty list of sources must not change the runtime registry");
        require(report.sources_attempted == 0 &&
                    report.sources_succeeded == 0 &&
                    report.devices_found == 0 &&
                    report.devices_processed == 0 && report.errors.empty(),
                "Scanning empty list of sources must return empty report");
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
                    smart_home::DiscoveredDevice{"SIM-LIGHT-003",
                                                 "unsupported.light", "Light5"},
                    smart_home::DiscoveredDevice{"SIM-LIGHT-002",
                                                 "simulated.light", "Light2"}},
                false));
        sources_list.push_back(
            std::make_unique<smart_home::SimulatedDiscoverySource>(
                std::vector<smart_home::DiscoveredDevice>{
                    smart_home::DiscoveredDevice{"SIM-LIGHT-001",
                                                 "simulated.light", "Light3"}},
                true));
        sources_list.push_back(
            std::make_unique<smart_home::SimulatedDiscoverySource>(
                std::vector<smart_home::DiscoveredDevice>{
                    smart_home::DiscoveredDevice{"SIM-LIGHT-001",
                                                 "simulated.light", "Light4"}},
                false));

        smart_home::DiscoveryService ds(device_manager,
                                        std::move(sources_list));

        auto report = ds.scanOnce();
        require(runtime_registry.size() == 2,
                "After scan not empty sources the runtime registry must change "
                "state correctly");
        require(persist_storage.getAll().size() == 2,
                "After scan not empty sources the persist storage must change "
                "state correctly");

        require(report.sources_attempted == 3 &&
                    report.sources_succeeded == 2 &&
                    report.devices_processed == 3 && report.devices_found == 4,
                "Second source must not make sense on next source");
        require(report.errors.size() == 2, "In report must be 2 errors");
        require(report.errors.at(0).source_index == 0 &&
                    report.errors.at(0).state ==
                        smart_home::DiscoveryState::Registration &&
                    report.errors.at(0).discovered_device ==
                        smart_home::DiscoveredDevice{
                            "SIM-LIGHT-003", "unsupported.light", "Light5"},
                "First error in report should be erorr with device");
        require(report.errors.at(1).source_index == 1 &&
                    report.errors.at(1).state ==
                        smart_home::DiscoveryState::Scan &&
                    report.errors.at(1).discovered_device == std::nullopt,
                "Second error in report should be erorr with scan");
        auto sim_light_1 = persist_storage.findByPhysicalId("SIM-LIGHT-001",
                                                            "simulated.light");
        require(sim_light_1.has_value(), "First light should be found");
        require(
            sim_light_1->name == "Light1",
            "First lisht should get correct name of the first correct device");

        auto sim_light_2 = persist_storage.findByPhysicalId("SIM-LIGHT-002",
                                                            "simulated.light");
        require(sim_light_2.has_value(), "Second light should be found");

        auto repeated_report = ds.scanOnce();
        require(
            runtime_registry.size() == 2,
            "After repeated scan the runtime registry must not change state");
        require(
            persist_storage.getAll().size() == 2,
            "After repeated scan the persist storage must not change state");

        require(repeated_report.sources_attempted == 3 &&
                    repeated_report.sources_succeeded == 2 &&
                    repeated_report.devices_processed == 3 &&
                    repeated_report.devices_found == 4,
                "Second source must not make sense on next source");
        require(repeated_report.errors.size() == 2,
                "In report must be 2 errors");
        require(repeated_report.errors.at(0).source_index == 0 &&
                    repeated_report.errors.at(0).state ==
                        smart_home::DiscoveryState::Registration &&
                    repeated_report.errors.at(0).discovered_device ==
                        smart_home::DiscoveredDevice{
                            "SIM-LIGHT-003", "unsupported.light", "Light5"},
                "First error in report should be erorr with device");
        require(repeated_report.errors.at(1).source_index == 1 &&
                    repeated_report.errors.at(1).state ==
                        smart_home::DiscoveryState::Scan &&
                    repeated_report.errors.at(1).discovered_device ==
                        std::nullopt,
                "Second error in report should be erorr with scan");

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
