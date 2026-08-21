#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "devices/interfaces/IOnOff.h"
#include "execution/CommandDispatcher.h"

#include <optional>
#include <stdexcept>
#include <variant>

namespace smart_home {

std::optional<Value>
CommandDispatcher::executeOnOffSet(Device &device,
                                   const OperationDescriptor &operation,
                                   const Command &command) {
    const auto &operation_input_parameter =
        validateSingleInputWrite(operation, command);

    const auto *operation_metadata =
        std::get_if<BooleanMetadata>(&operation_input_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "on_off.set descriptor must use BooleanMetadata");

    const Value &command_argument = command.getArguments().at(0);
    if (!std::holds_alternative<bool>(command_argument))
        throw std::invalid_argument("on_off.set expects a boolean argument");
    const bool command_argument_value = std::get<bool>(command_argument);

    auto *on_off_interface = dynamic_cast<IOnOff *>(&device);
    if (on_off_interface == nullptr)
        throw std::logic_error("Descriptor declares on_off.set, but device "
                               "does not implement IOnOff");
    on_off_interface->setOn(command_argument_value);
    return std::nullopt;
}

std::optional<Value>
CommandDispatcher::executeOnOffGet(Device &device,
                                   const OperationDescriptor &operation,
                                   const Command &command) {
    const auto &operation_output_parameter =
        validateNoInputRead(operation, command);

    const auto *operation_metadata =
        std::get_if<BooleanMetadata>(&operation_output_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "on_off.get descriptor must use BooleanMetadata");

    const auto *on_off_interface = dynamic_cast<const IOnOff *>(&device);
    if (on_off_interface == nullptr)
        throw std::logic_error("Descriptor declares on_off.get, but device "
                               "does not implement IOnOff");
    return Value{on_off_interface->isOn()};
}

} // namespace smart_home
