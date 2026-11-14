#include "OntologyTestSupport.h"

using namespace ontology_test;
int main() {
    try {
        Fixture fixture;
        fs::path runtime;
        {
            LadybugOntologyStorage storage(fixture.config);
            runtime = Access::runtimePath(storage);
            require(fs::exists(fixture.config.persistent_path), "initial durable schema snapshot");
            const auto before = bytes(fixture.config.persistent_path);
            seed(storage);
            require(bytes(fixture.config.persistent_path) == before && persistedRooms(fixture).empty(), "mutations remain in RAM");
            storage.sync();
            require(persistedRooms(fixture).size() == 2, "manual sync visible on disk");
            for (auto stage : {Stage::BeforeCopy, Stage::BeforeRename}) {
                const auto old = bytes(fixture.config.persistent_path);
                storage.addRoom({"pending", "Pending"});
                Access::setSnapshotHook(storage, [stage](auto point) {
                    if (point == stage) throw std::runtime_error("injected snapshot failure");
                });
                expectException<std::runtime_error>([&] { storage.sync(); });
                require(bytes(fixture.config.persistent_path) == old && storage.getRoom("pending"), "old disk snapshot and RAM survive copy failure");
                for (const auto& entry : fs::directory_iterator(fixture.root))
                    require(entry.path().filename().string().find(".home.lbdb.tmp_") != 0, "failed temporary snapshot cleaned");
                Access::setSnapshotHook(storage, {});
                storage.sync();
                require(persistedRooms(fixture).size() == 3, "sync retry");
            }
            storage.addRoom({"post_rename", "New"});
            Access::setSnapshotHook(storage, [](auto point) {
                if (point == Stage::AfterRename) throw std::runtime_error("injected directory durability failure");
            });
            try { storage.sync(); require(false, "post-rename failure not surfaced"); }
            catch (const std::runtime_error& error) {
                require(std::string(error.what()).find("durability is uncertain") != std::string::npos, "post-rename uncertainty distinguished");
            }
            Access::setSnapshotHook(storage, {});
            storage.sync();
            require(persistedRooms(fixture).size() == 4, "post-rename retry");
            storage.addRoom({"closing", "Closing"});
            Access::setSnapshotHook(storage, [](auto point) {
                if (point == Stage::BeforeRename) throw std::runtime_error("injected final sync failure");
            });
            expectException<std::runtime_error>([&] { storage.close(); });
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(fixture.config); });
            Access::setSnapshotHook(storage, {});
            storage.close();
            require(!fs::exists(runtime.parent_path()), "shutdown removes workspace");
        }
        {
            // Leftover RAM files never supersede the durable snapshot.
            fs::create_directory(fixture.root / "ram" / "stale");
            std::ofstream(fixture.root / "ram" / "stale" / "workspace.lbdb") << "stale data";
            std::ofstream(fixture.root / ".home.lbdb.tmp_ABC123") << "stale snapshot";
            LadybugOntologyStorage reopened(fixture.config);
            require(reopened.getAllRooms().size() == 5 && Access::runtimePath(reopened) != runtime, "startup copies authoritative disk");
            require(!fs::exists(fixture.root / ".home.lbdb.tmp_ABC123"), "stale snapshot cleanup");
            const auto details = reopened.getDeviceDetails(0).value();
            require(details.device == OntologyDeviceRecord{0, "Light"} && details.room_id == "living_room" &&
                    details.ability_ids == std::vector<std::string>{"brightness"}, "snapshot restores two-field device and links");
            reopened.addRoom({"destructor", "Final fallback"});
        }
        {
            LadybugOntologyStorage reopened(fixture.config);
            require(reopened.getRoom("destructor").has_value(), "destructor final sync");
            reopened.addRoom({"reopen_failure", "Saved before reopen"});
            Access::setSnapshotHook(reopened, [](auto point) {
                if (point == Stage::BeforeReopen) throw std::runtime_error("injected engine reopen failure");
            });
            expectException<std::runtime_error>([&] { reopened.sync(); });
            expectException<std::logic_error>([&] { reopened.getAllRooms(); });
            Access::setSnapshotHook(reopened, {});
            reopened.close();
        }
        {
            LadybugOntologyStorage reopened(fixture.config);
            require(reopened.getRoom("reopen_failure").has_value(), "snapshot survived failed reopen");
        }
        // Kill a separate writer after COMMIT, bypassing engine destructors to
        // produce a real WAL. Startup must replay it before the disk->RAM copy.
        const auto child = ::fork();
        if (child == 0) {
            try {
                rawDatabase(fixture.config.persistent_path, [](auto& conn) {
                    rawQuery(conn, "CREATE (:rooms {id: 'recovered', name: 'WAL recovery'})");
                    ::_exit(0);
                });
            } catch (...) { ::_exit(1); }
        }
        childSucceeded(child);
        require(fs::exists(fixture.config.persistent_path + ".wal"), "recovery fixture has WAL");
        {
            LadybugOntologyStorage recovered(fixture.config);
            require(recovered.getRoom("recovered").has_value(), "WAL replay precedes RAM copy");
            recovered.close();
        }
        {
            Fixture failure;
            fs::path retained;
            std::string baseline;
            {
                LadybugOntologyStorage storage(failure.config);
                retained = Access::runtimePath(storage);
                baseline = bytes(failure.config.persistent_path);
                storage.addRoom({"unsaved", "Retained after failed destructor"});
                Access::setSnapshotHook(storage, [](auto point) {
                    if (point == Stage::BeforeRename) throw std::runtime_error("permanent I/O failure");
                });
            }
            require(fs::exists(retained) && bytes(failure.config.persistent_path) == baseline,
                    "failed destructor retains RAM and previous snapshot");
            // Recover a COPY manually; a fresh normal startup still trusts disk.
            const auto rescued = failure.root / "rescued.lbdb";
            fs::copy_file(retained, rescued);
            auto config = failure.config;
            config.persistent_path = rescued.string();
            LadybugOntologyStorage recovery(config);
            require(recovery.getRoom("unsaved").has_value(), "retained RAM database is recoverable");
            recovery.close();
            LadybugOntologyStorage original(failure.config);
            require(!original.getRoom("unsaved"), "normal startup ignores unsaved RAM leftovers");
        }
        std::cout << "Ontology synchronization tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
