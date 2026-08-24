#pragma once 

#include "command/Value.h"
#include "common/DeviceId.h"

#include <string>
#include <vector>

namespace smart_home {

class Command {
private:
    DeviceId device_id;
    std::string operation_id;
    std::vector<Value> arguments;

public:
    Command(DeviceId, std::string, std::vector<Value>);

    DeviceId getDeviceId() const;
    const std::string& getOperationId() const;
    const std::vector<Value>& getArguments() const;
};

} // namespace smart_home
