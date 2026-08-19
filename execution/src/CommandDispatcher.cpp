#include "execution/CommandDispatcher.h"
#include "CapabilityDescriptor.h"
#include "DeviceDescriptor.h"
#include "OperationDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/interfaces/IBrightness.h"
#include "registry/DeviceRegistry.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace smart_home {

CommandDispatcher::CommandDispatcher(DeviceRegistry &c_registry)
    : registry(c_registry) {}

std::optional<Value> CommandDispatcher::execute(const Command &command) {
    auto *driver = registry.find(command.getDeviceId());
    if (driver == nullptr)
        throw std::runtime_error(
            "Device with local_id=" + std::to_string(command.getDeviceId()) +
            " is not found");
    auto *operation =
        findOperation(driver->descriptor(), command.getOperationId());
    if (operation == nullptr)
        throw std::invalid_argument("Operation with id '" +
                                    command.getOperationId() +
                                    "' is not found");
    if (operation->getId() == "brightness.set")
        return executeBrightnessSet(driver->device(), *operation, command);
    else if (operation->getId() == "brightness.get")
        return executeBrightnessGet(driver->device(), *operation, command);
    else
        throw std::runtime_error("Operation with id '" +
                                 command.getOperationId() +
                                 "' is not executable");
}

const OperationDescriptor *
CommandDispatcher::findOperation(const DeviceDescriptor &descriptor,
                                 const std::string &operation_id) const {
    for (const auto &capability : descriptor.getCapabilities()) {
        for (const auto &operation : capability.getOperations()) {
            if (operation.getId() == operation_id)
                return &operation;
        }
    }
    return nullptr;
}

std::optional<Value>
CommandDispatcher::executeBrightnessSet(Device &device,
                                        const OperationDescriptor &operation,
                                        const Command &command) {
    if (operation.getInputParameters().size() != 1)
        throw std::logic_error("Invalid brightness.set descriptor");
    const auto &operation_input_parameter =
        operation.getInputParameters().at(0);

    const auto *operation_metadata =
        std::get_if<IntegerMetadata>(&operation_input_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "brightness.set descriptor must use IntegerMetadata");

    if (operation.getType() != OperationType::Write)
        throw std::logic_error("brightness.set must be a Write operation");

    if (command.getArguments().size() != 1)
        throw std::invalid_argument(
            "brightness.set expects exactly one argument");
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
        static_cast<uint8_t>(command_argument_value));
    return std::nullopt;
}

std::optional<Value>
CommandDispatcher::executeBrightnessGet(const Device &device,
                                        const OperationDescriptor &operation,
                                        const Command &command) {
    if (!operation.getInputParameters().empty())
        throw std::logic_error(
            "brightness.get descriptor must not have input parameters");

    if (!command.getArguments().empty())
        throw std::invalid_argument(
            "brightness.get does not accept arguements");

    const auto &operation_output_parameter = operation.getOutputParameter();
    if (!operation_output_parameter.has_value())
        throw std::logic_error(
            "brightness.get descriptor must have an output parameter");

    const auto *operation_metadata = std::get_if<IntegerMetadata>(
        &operation_output_parameter->getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "brightness.get descriptor must use IntegerMetadata");

    if (operation.getType() != OperationType::Read)
        throw std::logic_error("brightness.get must be a Read operation");

    const auto *brightness_interface = dynamic_cast<const IBrightness *>(&device);
    if (brightness_interface == nullptr)
        throw std::logic_error("Descriptor declares brightness.set, but device "
                               "does not implement IBrightness");
    return Value{static_cast<int>(brightness_interface->getBrightness())};
}

} // namespace smart_home
