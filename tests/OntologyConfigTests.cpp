#include "OntologyTestSupport.h"
#include <cstdlib>
#include <optional>

using namespace ontology_test;
namespace {
struct Environment {
    std::vector<std::pair<std::string, std::optional<std::string>>> saved;
    Environment() {
        for (const auto* key : {"SMART_HOME_ONTOLOGY_PATH", "SMART_HOME_ONTOLOGY_RAM_DIR",
                                "SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK", "SMART_HOME_ONTOLOGY_SYNC_SECONDS"}) {
            const auto* value = std::getenv(key);
            saved.emplace_back(key, value ? std::optional<std::string>(value) : std::nullopt);
            ::unsetenv(key);
        }
    }
    ~Environment() {
        for (const auto& [key, value] : saved) {
            if (value) ::setenv(key.c_str(), value->c_str(), 1);
            else ::unsetenv(key.c_str());
        }
    }
};
void environmentConfig() {
    Environment environment;
    require(configFromEnvironment().dump_path.empty(), "default environment disables auto dump");
    ::setenv("SMART_HOME_ONTOLOGY_PATH", "custom.lbdb", 1);
    ::setenv("SMART_HOME_ONTOLOGY_RAM_DIR", "custom-ram", 1);
    ::setenv("SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK", "1", 1);
    ::setenv("SMART_HOME_ONTOLOGY_SYNC_SECONDS", "obsolete and ignored", 1);
    const auto config = configFromEnvironment();
    require(config.database_path == "custom.lbdb" && config.dump_path == "custom.lbdb" &&
            config.ram_workspace_base == "custom-ram" && config.allow_disk_fallback, "environment overrides");
    for (const auto* key : {"SMART_HOME_ONTOLOGY_PATH", "SMART_HOME_ONTOLOGY_RAM_DIR",
                            "SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK"}) {
        const std::string previous = std::getenv(key);
        ::setenv(key, "", 1);
        expectException<std::invalid_argument>([] { configFromEnvironment(); });
        ::setenv(key, previous.c_str(), 1);
    }
    ::setenv("SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK", "true", 1);
    expectException<std::invalid_argument>([] { configFromEnvironment(); });
}
} // namespace
int main() {
    try {
        OntologyConfig defaults;
        require(defaults.database_path == "data/home_ontology.lbdb" && defaults.ram_workspace_base == "/dev/shm" &&
                defaults.dump_path.empty() && !defaults.allow_disk_fallback && defaults.max_threads == 2 &&
                defaults.max_buffer_pool_bytes == 64 * 1024 * 1024, "defaults");
        validateConfig(defaults);
        for (int test = 0; test < 8; ++test) {
            auto config = defaults;
            if (test == 0) config.database_path.clear();
            if (test == 1) config.ram_workspace_base.clear();
            if (test == 2) config.dump_path = std::string("bad\0path", 8);
            if (test == 3) config.max_buffer_pool_bytes = 1;
            if (test == 4) config.max_threads = 0;
            if (test == 5) config.database_path = std::string("bad\0path", 8);
            if (test == 6) config.ram_workspace_base = std::string("bad\0path", 8);
            if (test == 7) config.dump_path = std::string(1, '\0');
            expectException<std::invalid_argument>([&] { validateConfig(config); });
            expectException<std::invalid_argument>([&] { LadybugOntologyStorage storage(config); });
        }
        environmentConfig();
        Fixture fixture;
        {
            LadybugOntologyStorage storage(fixture.config);
            const auto runtime = Access::runtimePath(storage);
            require(fs::exists(runtime) && runtime.parent_path().parent_path() == fixture.root / "ram", "private runtime path");
            require((fs::status(runtime.parent_path()).permissions() & fs::perms::all) == fs::perms::owner_all, "0700 workspace");
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(fixture.config); });
            fs::create_directory_symlink(fixture.root, fixture.root / "alias");
            auto alias = fixture.config;
            alias.database_path = (fixture.root / "alias" / "home.lbdb").string();
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(alias); });
            fs::create_symlink(fixture.config.database_path, fixture.root / "alias.lbdb");
            alias.database_path = (fixture.root / "alias.lbdb").string();
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(alias); });
            fs::create_hard_link(fixture.config.database_path, fixture.root / "hard.lbdb");
            alias.database_path = (fixture.root / "hard.lbdb").string();
            expectException<std::runtime_error>([&] { LadybugOntologyStorage other(alias); });
            fs::remove(fixture.root / "hard.lbdb");
            storage.close();
            require(!fs::exists(runtime.parent_path()), "workspace cleanup");
            storage.close();
            expectException<std::logic_error>([&] { storage.getAllRooms(); });
            expectException<std::logic_error>([&] { storage.dumpToFile(fixture.config.database_path); });
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
            std::ofstream(invalid.config.database_path) << "broken database";
            const auto before = bytes(invalid.config.database_path);
            expectException<std::exception>([&] { LadybugOntologyStorage storage(invalid.config); });
            require(bytes(invalid.config.database_path) == before && fs::is_empty(invalid.root / "ram"), "corruption preserved");
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
