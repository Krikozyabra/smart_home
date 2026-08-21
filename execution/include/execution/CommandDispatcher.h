#pragma once

#include "DeviceDescriptor.h"
#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "registry/DeviceRegistry.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace smart_home {

class CommandDispatcher {
  private:
    using Handler = std::optional<Value> (*)(Device &,
                                             const OperationDescriptor &,
                                             const Command &command);

    DeviceRegistry &registry;
    std::unordered_map<std::string, Handler> handlers;

    const OperationDescriptor *
    findOperation(const DeviceDescriptor &descriptor,
                  const std::string &operation_id) const;

    static const ParameterDescriptor &
    validateSingleInputWrite(const OperationDescriptor &operation,
                             const Command &command);

    static const ParameterDescriptor &
    validateNoInputRead(const OperationDescriptor &operation,
                        const Command &command);

    static std::optional<Value>
    executeBrightnessSet(Device &device, const OperationDescriptor &operation,
                         const Command &command);

    static std::optional<Value>
    executeBrightnessGet(Device &device, const OperationDescriptor &operation,
                         const Command &command);

    static std::optional<Value>
    executeTemperatureGet(Device &device, const OperationDescriptor &operation,
                          const Command &command);

    static std::optional<Value>
    executeOnOffGet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

    static std::optional<Value>
    executeOnOffSet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

    static std::optional<Value>
    executeColorGet(Device &device, const OperationDescriptor &operation,
                    const Command &command);
    
    static std::optional<Value>
    executeColorSet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

  public:
    explicit CommandDispatcher(DeviceRegistry &registry);

    std::optional<Value> execute(const Command &command);
};

} // namespace smart_home
