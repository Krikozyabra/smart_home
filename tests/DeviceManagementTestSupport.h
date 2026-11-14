#pragma once

#include "ontology/LadybugOntologyStorage.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace device_management_test {
namespace fs = std::filesystem;
using namespace smart_home::ontology;

// Uses only the public C++17 API; consumers do not need Ladybug SDK headers.
struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        auto pattern = (fs::temp_directory_path() / "smart-home-device-manager-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        const auto *directory = ::mkdtemp(buffer.data());
        if (!directory)
            throw std::runtime_error("Cannot create device manager test directory");
        path = directory;
        fs::create_directory(path / "ram");
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

inline OntologyConfig config(const fs::path &root) {
    OntologyConfig result;
    result.persistent_path = (root / "ontology.lbdb").string();
    result.ram_workspace_base = (root / "ram").string();
    result.sync_interval = std::chrono::seconds(0);
    return result;
}

inline void seedCatalog(IOntologyStorage &graph) {
    for (const auto &ability : {
             AbilityRecord{"on_off", "Power", "lighting"},
             AbilityRecord{"brightness", "Brightness", "lighting"},
             AbilityRecord{"color", "Color", "lighting"},
             AbilityRecord{"temperature", "Temperature", "climate"}})
        graph.addAbility(ability);
}

struct GraphFixture {
    TemporaryDirectory directory;
    LadybugOntologyStorage graph{config(directory.path)};
    GraphFixture() { seedCatalog(graph); }
};
} // namespace device_management_test
