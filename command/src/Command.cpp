#include "command/Command.h"
#include "command/Value.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

Command::Command(unsigned int c_device_id, std::string c_operation_id,
                 std::vector<Value> c_arguments)
    : device_id(c_device_id), operation_id(std::move(c_operation_id)),
      arguments(std::move(c_arguments)) {
    if (operation_id.empty())
        throw std::invalid_argument("Operation must not be empty");
}

unsigned int Command::getDeviceId() const { return device_id; }

const std::string &Command::getOperationId() const { return operation_id; }

const std::vector<Value> &Command::getArguments() const { return arguments; }

} // namespace smart_home
