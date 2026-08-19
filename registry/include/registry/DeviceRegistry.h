#pragma once

#include "drivers/interfaces/IDeviceDriver.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace smart_home {

class DeviceRegistry {
  private:
    std::unordered_map<unsigned int, std::unique_ptr<IDeviceDriver>> drivers;

  public:
    void add(std::unique_ptr<IDeviceDriver>);

    IDeviceDriver *find(unsigned int);
    const IDeviceDriver *find(unsigned int) const;

    std::size_t size() const;
};

} // namespace smart_home
