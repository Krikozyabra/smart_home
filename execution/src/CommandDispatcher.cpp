#include "execution/CommandDispatcher.h"

#include "drivers/CapabilityCatalog.h"
#include "logging/Logging.h"

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace smart_home {
namespace {

void validateParameter(const ParameterDescriptor &parameter,
                       const ParameterDescriptor &expected) {
    if (parameter.getValueType() != expected.getValueType())
        throw std::logic_error(
            "Operation parameter type disagrees with capability catalog");
    if (const auto *color =
            std::get_if<ColorMetadata>(&parameter.getMetadata());
        color && color->color_model != "RGB")
        throw std::logic_error("Color operation requires RGB");
}

} // namespace

CommandDispatcher::CommandDispatcher(DeviceRegistry &registry)
    : registry(registry) {}

std::optional<Value> CommandDispatcher::execute(const Command &command) {
    logging::Context context;
    context.device_id = command.getDeviceId();
    const auto &operation_id = command.getOperationId();
    context.operation = operation_id;
    logging::log(logging::Level::Debug, "CommandDispatcher",
                 "Dispatching command.", context);
    auto *driver = registry.find(command.getDeviceId());
    if (!driver)
        throw std::runtime_error(
            "Device with local_id=" + std::to_string(command.getDeviceId()) +
            " is not found");
    for (const auto &capability : driver->descriptor().getCapabilities())
        for (const auto &operation : capability.getOperations())
            if (operation.getId() == operation_id)
                return executeOperation(*driver, capability.getId(), operation,
                                        command);
    throw std::invalid_argument("Operation with id '" + operation_id +
                                "' is not found");
}

std::optional<Value> CommandDispatcher::executeOperation(
    IDeviceDriver &driver, const std::string &capability_id,
    const OperationDescriptor &operation, const Command &command) {
    const auto *entry = CapabilityCatalog::find(capability_id);
    if (!entry)
        throw std::logic_error("No executor registered for operation: " +
                               operation.getId());

    const auto &standard = entry->descriptor.getOperations();
    const auto expected =
        std::find_if(standard.begin(), standard.end(), [&](const auto &item) {
            return item.getId() == operation.getId();
        });
    if (expected == standard.end() ||
        expected->getType() != operation.getType())
        throw std::logic_error("Operation disagrees with capability catalog: " +
                               operation.getId());

    if (operation.getType() == OperationType::Read) {
        validateParameter(validateNoInputRead(operation, command),
                          expected->getOutputParameter().value());
        if (auto value = entry->read(driver.device()))
            return value;
        throw std::logic_error("Missing or invalid device property: " +
                               capability_id);
    }

    const auto &parameter = validateSingleInputWrite(operation, command);
    validateParameter(parameter, expected->getInputParameters().front());
    const auto &value = command.getArguments().front();
    if (!entry->accepts(value))
        throw std::invalid_argument("Invalid argument for operation: " +
                                    operation.getId());
    if (const auto *range =
            std::get_if<IntegerMetadata>(&parameter.getMetadata())) {
        const auto number = std::get<int>(value);
        if (number < range->min || number > range->max)
            throw std::invalid_argument("Argument outside descriptor range: " +
                                        operation.getId());
    }
    driver.writeCapability(capability_id, value);
    return std::nullopt;
}

} // namespace smart_home
