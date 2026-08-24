#pragma once

#include "common/DeviceId.h"
#include "drivers/interfaces/IDeviceDriver.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace smart_home {

class DeviceRegistry {
  private:
    std::unordered_map<DeviceId, std::unique_ptr<IDeviceDriver>> drivers;

  public:
    void add(std::unique_ptr<IDeviceDriver>);

    IDeviceDriver *find(DeviceId);
    const IDeviceDriver *find(DeviceId) const;

    std::size_t size() const;
};

} // namespace smart_home
