#include "registry/DriverFactoryRegistry.h"

#include <stdexcept>
#include <utility>

namespace smart_home {

void DriverFactoryRegistry::add(const std::string &driver_id,
                                DeviceCreator creator) {
    if (driver_id.empty() || !creator)
        throw std::invalid_argument(
            "Protocol ID and device creator must be provided");
    if (!creators_.try_emplace(driver_id, std::move(creator)).second)
        throw std::runtime_error("Device creator already registered: " +
                                 driver_id);
}

const DeviceCreator *
DriverFactoryRegistry::find(const std::string &driver_id) const {
    const auto found = creators_.find(driver_id);
    return found == creators_.end() ? nullptr : &found->second;
}

std::size_t DriverFactoryRegistry::size() const noexcept {
    return creators_.size();
}

} // namespace smart_home
