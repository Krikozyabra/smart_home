#include "devices/TemperatureSensor.h"

#include <string>
#include <utility>

namespace smart_home {

TemperatureSensor::TemperatureSensor(unsigned int c_id, std::string c_name, double c_temperature)
    : Device(c_id, std::move(c_name)), temperature(c_temperature) {}

double TemperatureSensor::getTemperature() const { return this->temperature; }

} // namespace smart_home
