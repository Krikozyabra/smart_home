#include "discovery/DiscoveryService.h"
#include "device_management/DeviceManager.h"
#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/entities/DiscoveryError.h"
#include "discovery/entities/DiscoveryReport.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include "logging/Logging.h"

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
    logging::log(logging::Level::Info, "Discovery", "Discovery started.");
    for (const auto &source : sources) {
        report.sources_attempted++;
        logging::Context context;
        context.source_index = report.sources_attempted - 1;
        logging::log(logging::Level::Trace, "Discovery", "Scanning source.", context);
        std::vector<DiscoveredDevice> device_list;
        try {
            device_list = source->scan();
            report.sources_succeeded++;
        } catch (const DiscoveryScanError &error) {
            logging::log(logging::Level::Warn, "Discovery", "Source scan failed; continuing.", context);
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
                logging::log(logging::Level::Warn, "Discovery", "Unsupported driver; skipping discovered device.", context);
                report.errors.push_back(DiscoveryError{
                    DiscoveryState::Registration, report.sources_attempted - 1,
                    device, error.what()});
            }
        }
    }
    logging::log(logging::Level::Info, "Discovery", "Discovery completed.");
    return report;
}

} // namespace smart_home
