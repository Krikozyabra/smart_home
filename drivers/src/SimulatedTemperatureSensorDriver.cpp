#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "CapabilityDescriptor.h"
#include "DeviceDescriptor.h"
#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "devices/TemperatureSensor.h"
#include "drivers/DeviceDriverBase.h"

#include <string>
#include <vector>
#include <memory>

namespace smart_home {

SimulatedTemperatureSensorDriver::SimulatedTemperatureSensorDriver(
    unsigned int c_local_id, std::string c_physical_id, std::string c_name,
    double c_temperature)
    : DeviceDriverBase(
          std::make_unique<TemperatureSensor>(c_local_id, c_name,
                                              c_temperature),
          generateDeviceDescriptor(c_local_id, c_physical_id, c_name)) {}

DeviceDescriptor SimulatedTemperatureSensorDriver::generateDeviceDescriptor(
    unsigned int local_id, const std::string &physical_id,
    const std::string &name) {

    ParameterDescriptor float_output("temperature.parameter.float",
                                     "Temperature", ValueType::Float,
                                     FloatMetadata{-30.0, 30.0, "C"});

    OperationDescriptor temperature_get(
        "temperature.get", "Get the device's temperature", OperationType::Read,
        std::vector<ParameterDescriptor>{}, float_output);

    CapabilityDescriptor temperature(
        "temperature", "Work with temperature of the device",
        std::vector<OperationDescriptor>{temperature_get});

    return DeviceDescriptor(local_id, physical_id, name,
                            std::vector<CapabilityDescriptor>{temperature});
}

} // namespace smart_home
