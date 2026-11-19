#include "ontology/OntologyConfig.h"
#include <cstdlib>
#include <stdexcept>

namespace smart_home::ontology {
void validateConfig(const OntologyConfig& config) {
    const auto validPath = [](const std::string& value) {
        return !value.empty() && value.find('\0') == std::string::npos;
    };
    if (!validPath(config.database_path) || !validPath(config.ram_workspace_base) ||
        config.dump_path.find('\0') != std::string::npos)
        throw std::invalid_argument("Ontology paths must be nonempty and contain no NUL.");
    if (config.max_buffer_pool_bytes < 16 * 1024 * 1024 || config.max_threads == 0)
        throw std::invalid_argument("Ontology requires at least 16 MiB of buffers and one engine thread.");
}

OntologyConfig configFromEnvironment() {
    OntologyConfig config;
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_PATH")) {
        config.database_path = value;
        config.dump_path = value;
    }
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_RAM_DIR")) config.ram_workspace_base = value;
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK")) {
        const std::string text(value);
        if (text != "0" && text != "1") throw std::invalid_argument("Ontology fallback must be 0 or 1.");
        config.allow_disk_fallback = text == "1";
    }
    validateConfig(config);
    return config;
}
} // namespace smart_home::ontology
