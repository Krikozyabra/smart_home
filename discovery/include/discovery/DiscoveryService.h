#pragma once

#include "device_management/DeviceManager.h"
#include "discovery/entities/DiscoveryReport.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include <memory>
#include <vector>

namespace smart_home {

class DiscoveryService {
  private:
    std::vector<std::unique_ptr<IDeviceDiscoverySource>> sources;
    DeviceManager &device_manager;

  public:
    DiscoveryService(
        DeviceManager &c_device_manager,
        std::vector<std::unique_ptr<IDeviceDiscoverySource>> c_sources);
    DiscoveryReport scanOnce();
};

} // namespace smart_home
