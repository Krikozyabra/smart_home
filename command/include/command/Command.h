#pragma once 

#include "command/Value.h"

#include <string>
#include <vector>

namespace smart_home {

class Command {
private:
    unsigned int device_id;
    std::string operation_id;
    std::vector<Value> arguments;

public:
    Command(unsigned int, std::string, std::vector<Value>);

    unsigned int getDeviceId() const;
    const std::string& getOperationId() const;
    const std::vector<Value>& getArguments() const;
};

} // namespace smart_home
