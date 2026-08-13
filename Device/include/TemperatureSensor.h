#pragma once

#include "Device.h"

class TemperatureSensor : public Device {
  public:
    TemperatureSensor(unsigned int c_id, std::string c_name)
        : Device(c_id, c_name), temperature(0) {}

    double getTemperature() { return this->temperature; }

  private:
    double temperature;
};
