#pragma once

#include "DeviceDescriptor.h"
#include "DeviceDriverBase.h"

#include <string>

namespace smart_home {

class SimulatedTemperatureSensorDriver final : public DeviceDriverBase{
  private:
    static DeviceDescriptor generateDeviceDescriptor(unsigned int, const std::string &,
                                              const std::string &);

  public:
    SimulatedTemperatureSensorDriver(unsigned int, std::string, std::string,
                                     double);
};

} // namespace smart_home
