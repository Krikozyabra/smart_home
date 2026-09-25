#include "discovery/DiscoveryService.h"
#include "device_management/DeviceManager.h"
#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace smart_home {

DiscoveryService::DiscoveryService(
    DeviceManager &c_device_manager,
    std::vector<std::unique_ptr<IDeviceDiscoverySource>> c_sources)
    : sources(std::move(c_sources)), device_manager(c_device_manager) {
    for (const auto &source : sources) {
        if (source == nullptr)
            throw std::invalid_argument("At least one source is nullptr");
    }
}

void DiscoveryService::scanOnce() {
    for (const auto &source : sources) {
        for (const auto &device : source->scan()) {
            device_manager.registerDiscoveredDevice(
                device.physical_id, device.driver_id, device.default_name);
        }
    }
}

} // namespace smart_home
