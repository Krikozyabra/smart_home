#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "devices/interfaces/IBrightness.h"
#include "execution/CommandDispatcher.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace smart_home {

std::optional<Value>
CommandDispatcher::executeBrightnessSet(Device &device,
                                        const OperationDescriptor &operation,
                                        const Command &command) {
    const auto &operation_input_parameter =
        validateSingleInputWrite(operation, command);

    const auto *operation_metadata =
        std::get_if<IntegerMetadata>(&operation_input_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "brightness.set descriptor must use IntegerMetadata");

    const Value &command_argument = command.getArguments().at(0);
    if (!std::holds_alternative<int>(command_argument))
        throw std::invalid_argument(
            "brightness.set expects an integer argument");
    const int command_argument_value = std::get<int>(command_argument);

    if (operation_metadata->min > command_argument_value ||
        operation_metadata->max < command_argument_value) {
        throw std::invalid_argument(
            "Command argument data is not in range [" +
            std::to_string(operation_metadata->min) + ";" +
            std::to_string(operation_metadata->max) + "]");
    }

    auto *brightness_interface = dynamic_cast<IBrightness *>(&device);
    if (brightness_interface == nullptr)
        throw std::logic_error("Descriptor declares brightness.set, but device "
                               "does not implement IBrightness");
    brightness_interface->setBrightness(
        static_cast<std::uint8_t>(command_argument_value));
    return std::nullopt;
}

std::optional<Value>
CommandDispatcher::executeBrightnessGet(Device &device,
                                        const OperationDescriptor &operation,
                                        const Command &command) {
    const auto &operation_output_parameter =
        validateNoInputRead(operation, command);

    const auto *operation_metadata =
        std::get_if<IntegerMetadata>(&operation_output_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "brightness.get descriptor must use IntegerMetadata");

    const auto *brightness_interface =
        dynamic_cast<const IBrightness *>(&device);
    if (brightness_interface == nullptr)
        throw std::logic_error("Descriptor declares brightness.get, but device "
                               "does not implement IBrightness");
    return Value{static_cast<int>(brightness_interface->getBrightness())};
}

} // namespace smart_home
