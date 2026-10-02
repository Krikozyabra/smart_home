#pragma once

#include "discovery/entities/DiscoveryError.h"

#include <cstddef>
#include <vector>

namespace smart_home {

struct DiscoveryReport {
    std::size_t sources_attempted = 0;
    std::size_t sources_succeeded = 0;
    std::size_t devices_found = 0;
    std::size_t devices_processed = 0;
    std::vector<DiscoveryError> errors{};
};

} // namespace smart_home
