#pragma once

#include "drivers/DeviceDriverBase.h"
#include "common/DeviceId.h"
#include "descriptors/DeviceDescriptor.h"

#include <string>

namespace smart_home {

class SimulatedTemperatureSensorDriver final : public DeviceDriverBase {
  private:
    static DeviceDescriptor generateDeviceDescriptor(DeviceId,
                                                     const std::string &,
                                                     const std::string &);

  public:
    SimulatedTemperatureSensorDriver(DeviceId, std::string, std::string,
                                     double);
};

} // namespace smart_home
