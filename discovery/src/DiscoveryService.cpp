#include "discovery/DiscoveryService.h"
#include "device_management/DeviceManager.h"
#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/entities/DiscoveryError.h"
#include "discovery/entities/DiscoveryReport.h"
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

DiscoveryReport DiscoveryService::scanOnce() {
    DiscoveryReport report;
    for (const auto &source : sources) {
        report.sources_attempted++;
        std::vector<DiscoveredDevice> device_list;
        try {
            device_list = source->scan();
            report.sources_succeeded++;
        } catch (const DiscoveryScanError &error) {
            report.errors.push_back(DiscoveryError{DiscoveryState::Scan,
                                                   report.sources_attempted - 1,
                                                   std::nullopt, error.what()});
            continue;
        }
        for (const auto &device : device_list) {
            report.devices_found++;
            try {
                device_manager.registerDiscoveredDevice(
                    device.physical_id, device.driver_id, device.default_name);
                report.devices_processed++;
            } catch (const UnsupportedDriverError &error) {
                report.errors.push_back(DiscoveryError{
                    DiscoveryState::Registration, report.sources_attempted - 1,
                    device, error.what()});
            }
        }
    }
    return report;
}

} // namespace smart_home
