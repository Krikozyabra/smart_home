#include "OntologyTestSupport.h"
#include <functional>

using namespace ontology_test;
namespace {
void requireClosed(IOntologyStorage& graph) {
    for (const auto& call : std::vector<std::function<void()>>{
        [&] { graph.addRoom({"r", "Room"}); }, [&] { graph.getRoom("r"); },
        [&] { graph.getAllRooms(); }, [&] { graph.deleteRoom("r"); },
        [&] { graph.addDevice({0, "Device"}); }, [&] { graph.getDevice(0); },
        [&] { graph.getAllDevices(); }, [&] { graph.deleteDevice(0); },
        [&] { graph.addAbility({"a", "Ability", "Category"}); }, [&] { graph.getAbility("a"); },
        [&] { graph.getAllAbilities(); }, [&] { graph.deleteAbility("a"); },
        [&] { graph.assignDeviceToRoom(0, "r"); }, [&] { graph.removeDeviceFromRoom(0); },
        [&] { graph.getDeviceRoom(0); }, [&] { graph.getDevicesInRoom("r"); },
        [&] { graph.addDeviceAbility(0, "a"); }, [&] { graph.removeDeviceAbility(0, "a"); },
        [&] { graph.getDeviceAbilities(0); }, [&] { graph.getDevicesWithAbility("a"); },
        [&] { graph.getDeviceDetails(0); }, [&] { graph.dumpToFile(""); }
    }) expectException<std::logic_error>(call);
    graph.close();
}

void relativePathsAndCloseRetry() {
    Fixture fixture;
    struct CurrentDirectory {
        fs::path previous = fs::current_path();
        ~CurrentDirectory() { std::error_code ignored; fs::current_path(previous, ignored); }
    } directory;
    fs::current_path(fixture.root);
    OntologyConfig config;
    config.database_path = "source.lbdb";
    config.dump_path = "automatic.lbdb";
    config.ram_workspace_base = "ram";
    LadybugOntologyStorage storage(config);
    require(fs::exists("automatic.lbdb") && !fs::exists("source.lbdb"), "initial schema dump uses separate final path");
    storage.addRoom({"room", "Room"});
    fs::current_path(fixture.root / "ram");
    storage.dumpToFile("manual.lbdb");
    require(fs::exists(fixture.root / "ram" / "manual.lbdb"), "explicit relative dump uses call directory");
    fs::remove(fixture.root / "automatic.lbdb");
    fs::create_directory(fixture.root / "automatic.lbdb");
    expectException<std::runtime_error>([&] { storage.close(); });
    require(storage.getRoom("room").has_value(), "invalidated final path leaves graph open");
    fs::remove(fixture.root / "automatic.lbdb");
    storage.close();
    require(fs::exists(fixture.root / "automatic.lbdb") && !fs::exists("automatic.lbdb"), "final path pinned at construction");
    config.database_path = (fixture.root / "automatic.lbdb").string();
    config.dump_path.clear();
    config.ram_workspace_base = (fixture.root / "ram").string();
    LadybugOntologyStorage reopened(config);
    require(reopened.getRoom("room").has_value(), "retry persisted graph");
}

void explicitDestinations() {
    Fixture fixture;
    fixture.config.dump_path.clear();
    const auto first = fixture.root / "first.lbdb";
    const auto second = fixture.root / "second.lbdb";
    fs::path runtime;
    std::string before;
    {
        LadybugOntologyStorage storage(fixture.config);
        runtime = Access::runtimePath(storage);
        require(!fs::exists(fixture.config.database_path), "no initial snapshot without dump_path");
        seed(storage);
        IOntologyStorage& graph = storage;
        const auto rooms = graph.getAllRooms();
        const auto devices = graph.getAllDevices();
        const auto abilities = graph.getAllAbilities();
        std::vector<DeviceOntologyDetails> deviceDetails;
        for (const auto& device : devices)
            deviceDetails.push_back(graph.getDeviceDetails(device.id).value());

        graph.dumpToFile(first.string());
        graph.dumpToFile(second.string()); // No intervening mutation: both destinations must be written.
        for (const auto& destination : {first, second}) {
            const auto label = "explicit snapshot " + destination.filename().string();
            require(fs::is_regular_file(destination), label + " was written");
            auto config = fixture.config;
            config.database_path = destination.string();
            LadybugOntologyStorage restored(config);
            require(restored.getAllRooms() == rooms, label + " restores rooms");
            require(restored.getAllDevices() == devices, label + " restores devices");
            require(restored.getAllAbilities() == abilities, label + " restores abilities");
            for (const auto& details : deviceDetails)
                require(restored.getDeviceDetails(details.device.id) == details,
                        label + " restores device relationships");
            restored.close();
        }
        before = bytes(first);
        graph.addRoom({"later", "Later"});
        expectException<std::invalid_argument>([&] { graph.dumpToFile(""); });
        expectException<std::invalid_argument>([&] { graph.dumpToFile(std::string("bad\0path", 8)); });
        expectException<std::invalid_argument>([&] { graph.dumpToFile(fixture.root.string()); });
        expectException<std::invalid_argument>([&] { graph.dumpToFile((fixture.root / "missing" / "db").string()); });
        expectException<std::invalid_argument>([&] { graph.dumpToFile(runtime.string()); });
        expectException<std::invalid_argument>([&] { graph.dumpToFile(fixture.config.database_path + ".lock"); });
        fs::create_symlink(first, fixture.root / "link");
        fs::create_symlink(fixture.root / "absent", fixture.root / "dangling");
        fs::create_hard_link(runtime, fixture.root / "runtime-alias");
        fs::create_hard_link(fixture.config.database_path + ".lock", fixture.root / "lock-alias");
        fs::create_hard_link(first, fixture.root / "hard-link");
        for (const auto* name : {"link", "dangling", "runtime-alias", "lock-alias", "hard-link"})
            expectException<std::invalid_argument>([&] { graph.dumpToFile((fixture.root / name).string()); });
        fs::remove(fixture.root / "runtime-alias");
        fs::remove(fixture.root / "lock-alias");
        fs::remove(fixture.root / "hard-link");
        require(bytes(first) == before && graph.getRoom("later"), "invalid destinations preserve files and live graph");
        graph.close();
        requireClosed(graph);
        require(!fs::exists(runtime.parent_path()) && !fs::exists(fixture.config.database_path) &&
                bytes(first) == before, "close without auto dump does not publish unsaved changes");
    }
    auto config = fixture.config;
    config.database_path = first.string();
    const auto automatic = fixture.root / "automatic.lbdb";
    config.dump_path = automatic.string();
    {
        LadybugOntologyStorage storage(config);
        require(storage.getAllRooms().size() == 2 && !storage.getRoom("later"), "explicit snapshot reloads graph");
        storage.addRoom({"final", "Final"});
        storage.dumpToFile(second.string());
        storage.addRoom({"after_manual", "After manual dump"});
        storage.close();
        require(bytes(first) == before, "separate auto dump preserves input database");
    }
    config.database_path = automatic.string();
    config.dump_path.clear();
    LadybugOntologyStorage restored(config);
    require(restored.getRoom("after_manual").has_value(), "manual dump does not redirect or cancel final dump");
    restored.close();
    // No implicit destructor dump when disabled, even after a successful manual dump.
    {
        LadybugOntologyStorage storage(fixture.config);
        storage.addRoom({"discarded", "Discarded"});
    }
    require(!fs::exists(fixture.config.database_path), "destructor respects disabled auto dump");
}
} // namespace
int main() {
    try {
        explicitDestinations();
        relativePathsAndCloseRetry();
        Fixture fixture;
        fs::path runtime;
        {
            LadybugOntologyStorage storage(fixture.config);
            runtime = Access::runtimePath(storage);
            require(fs::exists(fixture.config.database_path), "initial durable schema snapshot");
            const auto before = bytes(fixture.config.database_path);
            seed(storage);
            require(bytes(fixture.config.database_path) == before && persistedRooms(fixture).empty(), "mutations remain in RAM");
            storage.dumpToFile(fixture.config.database_path);
            require(persistedRooms(fixture).size() == 2, "manual dump visible on disk");
            for (auto stage : {Stage::BeforeCopy, Stage::BeforeRename}) {
                const auto old = bytes(fixture.config.database_path);
                storage.addRoom({"pending", "Pending"});
                Access::setSnapshotHook(storage, [stage](auto point) {
                    if (point == stage) throw std::runtime_error("injected snapshot failure");
                });
                expectException<std::runtime_error>([&] { storage.dumpToFile(fixture.config.database_path); });
                require(bytes(fixture.config.database_path) == old && storage.getRoom("pending"), "old disk snapshot and RAM survive copy failure");
                for (const auto& entry : fs::directory_iterator(fixture.root))
                    require(entry.path().filename().string().find(".home.lbdb.tmp_") != 0, "failed temporary snapshot cleaned");
                Access::setSnapshotHook(storage, {});
                storage.dumpToFile(fixture.config.database_path);
                require(persistedRooms(fixture).size() == 3, "dump retry");
            }
            storage.addRoom({"post_rename", "New"});
            Access::setSnapshotHook(storage, [](auto point) {
                if (point == Stage::AfterRename) throw std::runtime_error("injected directory durability failure");
            });
            try { storage.dumpToFile(fixture.config.database_path); require(false, "post-rename failure not surfaced"); }
            catch (const std::runtime_error& error) {
                require(std::string(error.what()).find("durability is uncertain") != std::string::npos, "post-rename uncertainty distinguished");
            }
            Access::setSnapshotHook(storage, {});
            storage.dumpToFile(fixture.config.database_path);
            require(persistedRooms(fixture).size() == 4, "post-rename retry");
            storage.addRoom({"closing", "Closing"});
            Access::setSnapshotHook(storage, [](auto point) {
                if (point == Stage::BeforeRename) throw std::runtime_error("injected final dump failure");
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
            require(reopened.getRoom("destructor").has_value(), "destructor final dump");
            reopened.addRoom({"reopen_failure", "Saved before reopen"});
            Access::setSnapshotHook(reopened, [](auto point) {
                if (point == Stage::BeforeReopen) throw std::runtime_error("injected engine reopen failure");
            });
            expectException<std::runtime_error>([&] { reopened.dumpToFile(fixture.config.database_path); });
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
                rawDatabase(fixture.config.database_path, [](auto& conn) {
                    rawQuery(conn, "CREATE (:rooms {id: 'recovered', name: 'WAL recovery'})");
                    ::_exit(0);
                });
            } catch (...) { ::_exit(1); }
        }
        childSucceeded(child);
        require(fs::exists(fixture.config.database_path + ".wal"), "recovery fixture has WAL");
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
                baseline = bytes(failure.config.database_path);
                storage.addRoom({"unsaved", "Retained after failed destructor"});
                Access::setSnapshotHook(storage, [](auto point) {
                    if (point == Stage::BeforeRename) throw std::runtime_error("permanent I/O failure");
                });
            }
            require(fs::exists(retained) && bytes(failure.config.database_path) == baseline,
                    "failed destructor retains RAM and previous snapshot");
            // Recover a COPY manually; a fresh normal startup still trusts disk.
            const auto rescued = failure.root / "rescued.lbdb";
            fs::copy_file(retained, rescued);
            auto config = failure.config;
            config.database_path = rescued.string();
            config.dump_path.clear();
            LadybugOntologyStorage recovery(config);
            require(recovery.getRoom("unsaved").has_value(), "retained RAM database is recoverable");
            recovery.close();
            LadybugOntologyStorage original(failure.config);
            require(!original.getRoom("unsaved"), "normal startup ignores unsaved RAM leftovers");
        }
        std::cout << "Ontology dump tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
