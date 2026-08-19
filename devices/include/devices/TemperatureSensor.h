#pragma once

#include "Device.h"
#include "interfaces/ITemperature.h"

namespace smart_home {

class TemperatureSensor : public Device, public ITemperature {
  public:
    TemperatureSensor(unsigned int, std::string, double);

    double getTemperature() const override;

  private:
    double temperature;
};

} // namespace smart_home
