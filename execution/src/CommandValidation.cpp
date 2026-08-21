#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "command/Command.h"
#include "execution/CommandDispatcher.h"

#include <stdexcept>

namespace smart_home {

const ParameterDescriptor &CommandDispatcher::validateSingleInputWrite(
    const OperationDescriptor &operation, const Command &command) {

    if (operation.getInputParameters().size() != 1)
        throw std::logic_error(
            operation.getId() +
            " descriptor must have exactly only input parameter");
    if (operation.getType() != OperationType::Write)
        throw std::logic_error(operation.getId() +
                               " descriptor must be a Write operation");
    if (operation.getOutputParameter().has_value())
        throw std::logic_error(operation.getId() +
                               " descriptor must not have output parameter");
    if (command.getArguments().size() != 1)
        throw std::invalid_argument(command.getOperationId() +
                                    " expects exactly one argument");
    return operation.getInputParameters().at(0);
}

const ParameterDescriptor &
CommandDispatcher::validateNoInputRead(const OperationDescriptor &operation,
                                       const Command &command) {

    if (!operation.getInputParameters().empty())
        throw std::logic_error(operation.getId() +
                               " descriptor must not have input parameters");
    if (operation.getType() != OperationType::Read)
        throw std::logic_error(operation.getId() +
                               " descriptor must be a Read operation");
    if (!command.getArguments().empty())
        throw std::invalid_argument(command.getOperationId() +
                                    " does not accept arguments");
    const auto &operation_output_parameter = operation.getOutputParameter();
    if (!operation_output_parameter.has_value())
        throw std::logic_error(operation.getId() +
                               " descriptor must have an output parameter");
    return *operation_output_parameter;
}

} // namespace smart_home
