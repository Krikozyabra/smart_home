#pragma once

#include "DeviceDescriptor.h"
#include "OperationDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "registry/DeviceRegistry.h"
#include <optional>
#include <string>

namespace smart_home {

class CommandDispatcher {
  private:
    DeviceRegistry &registry;

    const OperationDescriptor *
    findOperation(const DeviceDescriptor &descriptor,
                  const std::string &operation_id) const;

    std::optional<Value>
    executeBrightnessSet(Device &device, const OperationDescriptor &operation,
                         const Command &command);

    std::optional<Value>
    executeBrightnessGet(const Device &device, const OperationDescriptor &operation,
                         const Command &command);

    std::optional<Value>
    executeTemperatureGet(const Device &device, const OperationDescriptor &operation,
                          const Command &command);

  public:
    explicit CommandDispatcher(DeviceRegistry &registry);

    std::optional<Value> execute(const Command &command);
};

} // namespace smart_home
