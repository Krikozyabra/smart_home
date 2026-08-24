#pragma once

#include "common/DeviceId.h"
#include "devices/types/Color.h"
#include "DeviceDescriptor.h"
#include "DeviceDriverBase.h"

#include <string>
#include <cstdint>

namespace smart_home {

class SimulatedLightDriver final : public DeviceDriverBase{
  private:
    static DeviceDescriptor generateDeviceDescriptor(DeviceId, const std::string &, const std::string &);

  public:
    SimulatedLightDriver(DeviceId, std::string, std::string, bool, uint8_t, Color);
};

} // namespace smart_home
