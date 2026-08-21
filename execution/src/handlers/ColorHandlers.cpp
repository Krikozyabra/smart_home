#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "devices/interfaces/IColor.h"
#include "devices/types/Color.h"
#include "execution/CommandDispatcher.h"

#include <optional>
#include <stdexcept>
#include <variant>

namespace smart_home {

std::optional<Value>
CommandDispatcher::executeColorSet(Device &device,
                                   const OperationDescriptor &operation,
                                   const Command &command) {
    const auto &operation_input_parameter =
        validateSingleInputWrite(operation, command);

    const auto *operation_metadata =
        std::get_if<ColorMetadata>(&operation_input_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error("color.set descriptor must use ColorMetadata");

    if (operation_metadata->color_model != "RGB")
        throw std::logic_error("color.set supports only RGB model");

    const Value &command_argument = command.getArguments().at(0);
    if (!std::holds_alternative<Color>(command_argument))
        throw std::invalid_argument("color.set expects a color argument");
    const Color &command_argument_value = std::get<Color>(command_argument);

    auto *color_interface = dynamic_cast<IColor *>(&device);
    if (color_interface == nullptr)
        throw std::logic_error("Descriptor declares color.set, but device "
                               "does not implement IColor");
    color_interface->setColor(command_argument_value);
    return std::nullopt;
}

std::optional<Value>
CommandDispatcher::executeColorGet(Device &device,
                                   const OperationDescriptor &operation,
                                   const Command &command) {
    const auto &operation_output_parameter =
        validateNoInputRead(operation, command);

    const auto *operation_metadata =
        std::get_if<ColorMetadata>(&operation_output_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error("color.get descriptor must use ColorMetadata");
    if (operation_metadata->color_model != "RGB")
        throw std::logic_error("color.get supports only RGB");

    const auto *color_interface = dynamic_cast<const IColor *>(&device);
    if (color_interface == nullptr)
        throw std::logic_error("Descriptor declares color.get, but device "
                               "does not implement IColor");
    return Value{color_interface->getColor()};
}

} // namespace smart_home
