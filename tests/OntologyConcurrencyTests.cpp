#include "OntologyTestSupport.h"
#include <atomic>
#include <thread>

using namespace ontology_test;
int main(int argc, char* argv[]) {
    try {
        if (argc == 3) {
            OntologyConfig config;
            config.persistent_path = argv[1];
            config.ram_workspace_base = argv[2];
            try { LadybugOntologyStorage forbidden(config); }
            catch (const std::runtime_error&) { return 0; }
            return 1;
        }
        Fixture fixture;
        fixture.config.sync_interval = std::chrono::seconds(30);
        LadybugOntologyStorage storage(fixture.config);
        seed(storage);
        const auto child = ::fork();
        if (child == 0) {
            ::execl(argv[0], argv[0], fixture.config.persistent_path.c_str(), fixture.config.ram_workspace_base.c_str(), nullptr);
            ::_exit(2);
        }
        childSucceeded(child);
        std::atomic<int> errors{0};
        std::vector<std::thread> clients;
        for (int index = 0; index < 4; ++index) {
            clients.emplace_back([&, index] {
                try {
                    for (int iteration = 0; iteration < 15; ++iteration) {
                        storage.addDevice({100 + index, "Concurrent"});
                        storage.assignDeviceToRoom(100 + index, "bathroom");
                        storage.addDeviceAbility(100 + index, "temperature");
                        const auto details = storage.getDeviceDetails(100 + index);
                        require(details && details->device == OntologyDeviceRecord{100 + index, "Concurrent"} &&
                                details->room_id == "bathroom" && details->ability_ids == std::vector<std::string>{"temperature"},
                                "concurrent details");
                    }
                } catch (...) { ++errors; }
            });
        }
        Access::triggerPeriodicSync(storage); // Same worker path as the timer, no sleep.
        for (auto& thread : clients) thread.join();
        require(errors == 0, "concurrent query failures");
        Access::triggerPeriodicSync(storage);
        require(persistedRooms(fixture).size() == 2, "periodic snapshot persisted");
        storage.addRoom({"retry", "Background retry"});
        Access::setSnapshotHook(storage, [](auto stage) {
            if (stage == Stage::BeforeRename) throw std::runtime_error("injected worker failure");
        });
        Access::triggerPeriodicSync(storage);
        require(persistedRooms(fixture).size() == 2, "worker failure preserves durable snapshot");
        Access::setSnapshotHook(storage, {});
        Access::triggerPeriodicSync(storage);
        require(persistedRooms(fixture).size() == 3, "worker retries after failure");
        std::thread first([&] { storage.close(); });
        std::thread second([&] { storage.close(); });
        first.join(); second.join();
        require(fs::is_empty(fixture.root / "ram"), "workers joined and workspace cleaned");
        // Verify an actual timer deadline in addition to the deterministic trigger.
        fixture.config.sync_interval = std::chrono::seconds(1);
        LadybugOntologyStorage timer(fixture.config);
        timer.addRoom({"timer", "Actual timer"});
        Access::waitForPeriodicSync(timer);
        require(persistedRooms(fixture).size() == 4, "real periodic deadline");
        timer.close();
        std::cout << "Ontology concurrency tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
