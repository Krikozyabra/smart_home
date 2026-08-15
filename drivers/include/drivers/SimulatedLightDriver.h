#pragma once

#include "Light.h"
#include "DeviceDescriptor.h"
#include "drivers/interfaces/IDeviceDriver.h"

#include <string>

namespace smart_home {

class SimulatedLightDriver final : public IDeviceDriver {
  private:
    Light device_object;
    DeviceDescriptor descriptor_object;

    DeviceDescriptor generateDeviceDescriptor(unsigned int, const std::string &, const std::string &);
    Light generateDevice(unsigned int, const std::string &);

  public:
    SimulatedLightDriver(unsigned int, std::string, std::string);
    Device &device() override;
    const DeviceDescriptor &descriptor() const override;
};

} // namespace smart_home
