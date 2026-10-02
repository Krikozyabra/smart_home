#pragma once

#include "discovery/entities/DiscoveredDevice.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

namespace smart_home {

class DiscoveryScanError : public std::runtime_error {
  public:
    explicit DiscoveryScanError(const std::string &message)
        : std::runtime_error(message) {}
};

enum class DiscoveryState { Scan, Registration };

struct DiscoveryError {
    DiscoveryState state;
    std::size_t source_index;
    std::optional<DiscoveredDevice> discovered_device;
    std::string message;
};

} // namespace smart_home
