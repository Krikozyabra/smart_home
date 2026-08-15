#pragma once

#include "DescriptorTypes.h"
#include "ParameterDescriptor.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

class OperationDescriptor {
    std::string id;
    std::string name;
    OperationType type;

    std::vector<ParameterDescriptor> input;
    std::optional<ParameterDescriptor> output;

  public:
    OperationDescriptor(std::string c_id, std::string c_name,
                        OperationType c_type,
                        std::vector<ParameterDescriptor> c_input,
                        std::optional<ParameterDescriptor> c_output)
        : id(std::move(c_id)), name(std::move(c_name)), type(c_type),
          input(std::move(c_input)), output(std::move(c_output)) {}

    const std::string& getId() const {
        return this->id;
    }

    const std::string& getName() const {
        return this->name;
    }

    OperationType getType() const {
        return this->type;
    }

    const std::vector<ParameterDescriptor>& getInputParameters() const {
        return this->input;
    }

    const std::optional<ParameterDescriptor>& getOutputParameter() const {
        return this->output;
    }
};

} // namespace smart_home
