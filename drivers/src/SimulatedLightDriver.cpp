#include "drivers/SimulatedLightDriver.h"
#include "CapabilityDescriptor.h"
#include "DeviceDescriptor.h"
#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "devices/Light.h"
#include "devices/types/Color.h"
#include "drivers/DeviceDriverBase.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace smart_home {

DeviceDescriptor
SimulatedLightDriver::generateDeviceDescriptor(unsigned int local_id,
                                               const std::string &physical_id,
                                               const std::string &name) {

    ParameterDescriptor bool_input_output{"on_off.parameter.bool", "Power",
                                          ValueType::Boolean,
                                          BooleanMetadata{}};
    OperationDescriptor on_off_set(
        "on_off.set", "Set the device's power", OperationType::Write,
        std::vector<ParameterDescriptor>{bool_input_output}, std::nullopt);
    OperationDescriptor on_off_get(
        "on_off.get", "Get the device's power status", OperationType::Read,
        std::vector<ParameterDescriptor>{}, bool_input_output);

    CapabilityDescriptor on_off(
        "on_off", "Work with power of the device",
        std::vector<OperationDescriptor>{on_off_set, on_off_get});

    ParameterDescriptor integer_input_output{"brightness.parameter.integer",
                                             "Brightness", ValueType::Integer,
                                             IntegerMetadata{0, 100, "%"}};
    OperationDescriptor brightness_set(
        "brightness.set", "Set the device's brightness", OperationType::Write,
        std::vector<ParameterDescriptor>{integer_input_output}, std::nullopt);
    OperationDescriptor brightness_get(
        "brightness.get", "Get the device's brightness", OperationType::Read,
        std::vector<ParameterDescriptor>{}, integer_input_output);

    CapabilityDescriptor brightness(
        "brightness", "Work with device's brightness",
        std::vector<OperationDescriptor>{brightness_set, brightness_get});

    ParameterDescriptor color_input_output{"color.parameter.color", "Color",
                                           ValueType::Color,
                                           ColorMetadata{"RGB"}};
    OperationDescriptor color_set(
        "color.set", "Set the device's color", OperationType::Write,
        std::vector<ParameterDescriptor>{color_input_output}, std::nullopt);

    OperationDescriptor color_get(
        "color.get", "Get the device's color", OperationType::Read,
        std::vector<ParameterDescriptor>{}, color_input_output);
    CapabilityDescriptor color(
        "color", "Work with device's color",
        std::vector<OperationDescriptor>{color_set, color_get});

    return DeviceDescriptor(
        local_id, physical_id, name,
        std::vector<CapabilityDescriptor>{on_off, brightness, color});
}

SimulatedLightDriver::SimulatedLightDriver(unsigned int c_local_id,
                                           std::string c_physical_id,
                                           std::string c_name, bool c_isOn,
                                           uint8_t c_brightness, Color c_color)
    : DeviceDriverBase(
          std::make_unique<Light>(c_local_id, c_name, c_isOn, c_brightness,
                                  c_color),
          generateDeviceDescriptor(c_local_id, c_physical_id, c_name)) {}
} // namespace smart_home
