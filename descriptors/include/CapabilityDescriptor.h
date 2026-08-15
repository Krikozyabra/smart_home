#pragma once

#include "OperationDescriptor.h"
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

class CapabilityDescriptor {
    std::string id;
    std::string name;

    std::vector<OperationDescriptor> operations;

  public:
    CapabilityDescriptor(std::string c_id, std::string c_name,
                         std::vector<OperationDescriptor> c_operations)
        : id(std::move(c_id)), name(std::move(c_name)),
          operations(std::move(c_operations)) {}

    const std::string& getId() const {
        return this->id;
    }

    const std::string& getName() const {
        return this->name;
    }

    const std::vector<OperationDescriptor>& getOperations() const {
        return this->operations;
    }
};

} // namespace smart_home
