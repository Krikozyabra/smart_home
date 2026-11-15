#include "OntologyTestSupport.h"
#include <atomic>
#include <future>
#include <thread>

using namespace ontology_test;
int main(int argc, char* argv[]) {
    try {
        if (argc == 3) {
            OntologyConfig config;
            config.database_path = argv[1];
            config.ram_workspace_base = argv[2];
            try { LadybugOntologyStorage forbidden(config); }
            catch (const std::runtime_error&) { return 0; }
            return 1;
        }
        Fixture fixture;
        LadybugOntologyStorage storage(fixture.config);
        seed(storage);
        const auto child = ::fork();
        if (child == 0) {
            ::execl(argv[0], argv[0], fixture.config.database_path.c_str(), fixture.config.ram_workspace_base.c_str(), nullptr);
            ::_exit(2);
        }
        childSucceeded(child);
        std::atomic<int> errors{0};
        std::promise<void> start;
        const auto ready = start.get_future().share();
        std::vector<std::thread> clients;
        for (int index = 0; index < 4; ++index) {
            clients.emplace_back([&, index] {
                ready.wait();
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
        for (int index = 0; index < 2; ++index) {
            clients.emplace_back([&, index] {
                ready.wait();
                try {
                    for (int iteration = 0; iteration < 5; ++iteration)
                        storage.dumpToFile((fixture.root / ("snapshot-" + std::to_string(index) + ".lbdb")).string());
                } catch (...) { ++errors; }
            });
        }
        start.set_value();
        for (auto& thread : clients) thread.join();
        require(errors == 0, "concurrent query/dump failures");
        storage.dumpToFile(fixture.config.database_path);
        require(persistedRooms(fixture).size() == 2, "explicit snapshot persisted");
        // Close must wait for an in-flight snapshot, then serialize other closes.
        std::promise<void> copying, release;
        const auto released = release.get_future().share();
        bool first_copy = true; // Hook invocations are serialized by transactionMutex.
        Access::setSnapshotHook(storage, [&](auto stage) {
            if (stage == Stage::BeforeCopy && first_copy) {
                first_copy = false; copying.set_value(); released.wait();
            }
        });
        std::thread dumping([&] {
            try { storage.dumpToFile((fixture.root / "closing.lbdb").string()); }
            catch (...) { ++errors; }
        });
        copying.get_future().wait();
        const auto close = [&] { try { storage.close(); } catch (...) { ++errors; } };
        std::thread first(close), second(close);
        release.set_value();
        dumping.join(); first.join(); second.join();
        require(errors == 0 && fs::is_empty(fixture.root / "ram"), "concurrent close cleans workspace");
        expectException<std::logic_error>([&] { storage.getAllDevices(); });
        LadybugOntologyStorage reopened(fixture.config);
        for (int index = 0; index < 4; ++index)
            require(reopened.getDeviceDetails(100 + index)->room_id == "bathroom", "final close persisted concurrent writes");
        std::cout << "Ontology concurrency tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
