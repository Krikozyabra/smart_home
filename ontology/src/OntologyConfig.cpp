#include "ontology/OntologyConfig.h"
#include <charconv>
#include <cstdlib>
#include <stdexcept>

namespace smart_home::ontology {
void validateConfig(const OntologyConfig& config) {
    const auto validPath = [](const std::string& value) {
        return !value.empty() && value.find('\0') == std::string::npos;
    };
    if (!validPath(config.persistent_path) || !validPath(config.ram_workspace_base))
        throw std::invalid_argument("Ontology paths must be nonempty and contain no NUL.");
    if (config.sync_interval.count() < 0 || config.sync_interval > std::chrono::hours(24 * 365))
        throw std::invalid_argument("Ontology sync interval must be between 0 and 31536000 seconds.");
    if (config.max_buffer_pool_bytes < 16 * 1024 * 1024 || config.max_threads == 0)
        throw std::invalid_argument("Ontology requires at least 16 MiB of buffers and one engine thread.");
}

std::chrono::seconds parseSyncInterval(std::string_view value) {
    if (value.empty()) throw std::invalid_argument("Ontology sync interval must not be empty.");
    long long count = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), count);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        count < 0 || count > 31536000)
        throw std::invalid_argument("Invalid ontology sync interval (expected 0..31536000 seconds).");
    return std::chrono::seconds(count);
}

OntologyConfig configFromEnvironment() {
    OntologyConfig config;
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_PATH")) config.persistent_path = value;
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_RAM_DIR")) config.ram_workspace_base = value;
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_SYNC_SECONDS")) config.sync_interval = parseSyncInterval(value);
    if (const auto* value = std::getenv("SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK")) {
        const std::string text(value);
        if (text != "0" && text != "1") throw std::invalid_argument("Ontology fallback must be 0 or 1.");
        config.allow_disk_fallback = text == "1";
    }
    validateConfig(config);
    return config;
}
} // namespace smart_home::ontology
