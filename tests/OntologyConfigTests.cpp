#include "OntologyTestSupport.h"
#include <cstdlib>
#include <limits>

using namespace ontology_test;
int main() {
    try {
        OntologyConfig defaults;
        require(defaults.persistent_path == "data/home_ontology.lbdb" && defaults.ram_workspace_base == "/dev/shm" &&
                defaults.sync_interval == std::chrono::seconds(30) && !defaults.allow_disk_fallback, "defaults");
        require(parseSyncInterval("0").count() == 0 && parseSyncInterval("31536000").count() == 31536000, "interval limits");
        for (const auto* bad : {"", "-1", "1x", "1.5", "+3", " 4", "31536001", "999999999999999999999999"})
            expectException<std::invalid_argument>([&] { parseSyncInterval(bad); });
        for (int test = 0; test < 5; ++test) {
            auto config = defaults;
            if (test == 0) config.persistent_path.clear();
            if (test == 1) config.ram_workspace_base.clear();
            if (test == 2) config.sync_interval = std::chrono::seconds(-1);
            if (test == 3) config.max_buffer_pool_bytes = 1;
            if (test == 4) config.max_threads = 0;
            expectException<std::invalid_argument>([&] { LadybugOntologyStorage storage(config); });
        }
        Fixture fixture;
        {
            LadybugOntologyStorage storage(fixture.config);
            const auto runtime = Access::runtimePath(storage);
            require(fs::exists(runtime) && runtime.parent_path().parent_path() == fixture.root / "ram", "private runtime path");
            require((fs::status(runtime.parent_path()).permissions() & fs::perms::all) == fs::perms::owner_all, "0700 workspace");
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(fixture.config); });
            fs::create_directory_symlink(fixture.root, fixture.root / "alias");
            auto alias = fixture.config;
            alias.persistent_path = (fixture.root / "alias" / "home.lbdb").string();
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(alias); });
            fs::create_symlink(fixture.config.persistent_path, fixture.root / "alias.lbdb");
            alias.persistent_path = (fixture.root / "alias.lbdb").string();
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(alias); });
            fs::create_hard_link(fixture.config.persistent_path, fixture.root / "hard.lbdb");
            alias.persistent_path = (fixture.root / "hard.lbdb").string();
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(alias); });
            fs::remove(fixture.root / "hard.lbdb");
            storage.close();
            require(!fs::exists(runtime.parent_path()), "workspace cleanup");
            storage.close();
            expectException<std::logic_error>([&] { storage.getAllRooms(); });
            expectException<std::logic_error>([&] { storage.sync(); });
        }
        // The persistent lock inode must survive close and atomic file replacement.
        LadybugOntologyStorage reopened(fixture.config);
        reopened.close();
        auto missingRam = fixture.config;
        missingRam.ram_workspace_base = (fixture.root / "missing").string();
        expectException<fs::filesystem_error>([&] { LadybugOntologyStorage storage(missingRam); });
        missingRam.allow_disk_fallback = true;
        LadybugOntologyStorage fallback(missingRam);
        require(Access::runtimePath(fallback).parent_path().parent_path() == fixture.root, "explicit fallback workspace");
        fallback.close();
        {
            Fixture invalid;
            std::ofstream(invalid.config.persistent_path) << "broken database";
            const auto before = bytes(invalid.config.persistent_path);
            expectException<std::exception>([&] { LadybugOntologyStorage storage(invalid.config); });
            require(bytes(invalid.config.persistent_path) == before && fs::is_empty(invalid.root / "ram"), "corruption preserved");
        }
        // Real /dev/shm smoke test. The rest uses isolated injectable roots so it
        // remains runnable in containers that prohibit writes outside /tmp.
        {
            std::error_code error;
            const auto probe = fs::path("/dev/shm") / ("smart-home-probe-" + std::to_string(::getpid()));
            if (fs::create_directory(probe, error)) {
                fs::remove(probe);
                Fixture smoke;
                smoke.config.ram_workspace_base = "/dev/shm";
                LadybugOntologyStorage storage(smoke.config);
                require(Access::runtimePath(storage).parent_path().parent_path() == "/dev/shm", "real tmpfs runtime");
                storage.addRoom({"smoke", "Tmpfs"});
                storage.close();
            } else std::cout << "SKIP /dev/shm smoke: " << error.message() << '\n';
        }
        std::cout << "Ontology configuration tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
