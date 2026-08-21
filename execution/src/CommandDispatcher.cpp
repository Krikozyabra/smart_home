#include "execution/CommandDispatcher.h"
#include "CapabilityDescriptor.h"
#include "DeviceDescriptor.h"
#include "OperationDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "registry/DeviceRegistry.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace smart_home {

CommandDispatcher::CommandDispatcher(DeviceRegistry &c_registry)
    : registry(c_registry),
      handlers{{"brightness.set", &CommandDispatcher::executeBrightnessSet},
               {"brightness.get", &CommandDispatcher::executeBrightnessGet},
               {"temperature.get", &CommandDispatcher::executeTemperatureGet},
               {"on_off.get", &CommandDispatcher::executeOnOffGet},
               {"on_off.set", &CommandDispatcher::executeOnOffSet},
               {"color.get", &CommandDispatcher::executeColorGet},
               {"color.set", &CommandDispatcher::executeColorSet}} {}

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
    auto handler_iterator = handlers.find(operation->getId());

    if (handler_iterator == handlers.end())
        throw std::logic_error("No executor registered for operation: " +
                               command.getOperationId());

    Handler handler = handler_iterator->second;

    return handler(driver->device(), *operation, command);
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

} // namespace smart_home
