#pragma once
#include "TestUtil.h"
#include "ontology/LadybugOntologyStorage.h"
#include "Testing.h"
#include <lbug.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ontology_test {
namespace fs = std::filesystem;
using namespace smart_home::ontology;
using Access = detail::OntologyTestAccess;
using Stage = detail::SnapshotStage;
struct Fixture {
    fs::path root;
    OntologyConfig config;
    Fixture() {
        auto pattern = (fs::temp_directory_path() / "smart-home-ontology-test-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        auto* directory = ::mkdtemp(buffer.data());
        if (!directory) throw std::runtime_error("Cannot create test fixture");
        root = directory;
        fs::create_directory(root / "ram");
        config.persistent_path = (root / "home.lbdb").string();
        config.ram_workspace_base = (root / "ram").string();
        config.sync_interval = std::chrono::seconds(0);
    }
    ~Fixture() { std::error_code error; fs::remove_all(root, error); }
};
inline std::string bytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read fixture");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
inline void rawQuery(lbug::main::Connection& connection, const std::string& cypher) {
    const auto result = connection.query(cypher);
    require(result && result->isSuccess(), result ? result->getErrorMessage() : "No result");
}
template<class F> void rawDatabase(const fs::path& path, F operation) {
    lbug::main::SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 2;
    config.autoCheckpoint = false;
    config.forceCheckpointOnClose = true;
    lbug::main::Database db(path.string(), config);
    lbug::main::Connection connection(&db);
    operation(connection);
}
inline void seed(LadybugOntologyStorage& storage) {
    storage.addRoom({"living_room", "Гостиная"});
    storage.addRoom({"bathroom", "Ванная"});
    storage.addDevice({0, "Light"});
    storage.addDevice({1, "Temperature"});
    storage.addAbility({"brightness", "Яркость", "lighting"});
    storage.addAbility({"temperature", "Температура", "climate"});
    storage.assignDeviceToRoom(0, "living_room");
    storage.assignDeviceToRoom(1, "bathroom");
    storage.addDeviceAbility(0, "brightness");
    storage.addDeviceAbility(1, "temperature");
}
// Inspect a COPY of the durable snapshot without violating the owner's writer lock.
inline std::vector<RoomRecord> persistedRooms(Fixture& fixture) {
    const auto copy = fixture.root / "inspection.lbdb";
    fs::copy_file(fixture.config.persistent_path, copy, fs::copy_options::overwrite_existing);
    auto config = fixture.config;
    config.persistent_path = copy.string();
    config.sync_interval = std::chrono::seconds(0);
    LadybugOntologyStorage reader(config);
    auto result = reader.getAllRooms();
    reader.close();
    return result;
}
inline void childSucceeded(pid_t child) {
    require(child > 0, "fork failed");
    int status = 0;
    require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "child failed");
}
} // namespace ontology_test
