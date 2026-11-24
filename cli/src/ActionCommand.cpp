#include "cli/ActionCommand.h"

#include <stdexcept>
#include <utility>

namespace smart_home::cli {

ActionCommand::ActionCommand(std::string name, std::size_t expected_args,
                             Handler handler)
    : name_(std::move(name)), expected_args_(expected_args),
      handler_(std::move(handler)) {
    if (name_.empty())
        throw std::invalid_argument("Command name cannot be empty.");
    if (!handler_)
        throw std::invalid_argument("Command handler cannot be null.");
}

std::string_view ActionCommand::name() const noexcept { return name_; }

bool ActionCommand::validate(const CliArguments &args,
                             std::string &error) const {
    if (args.size() == expected_args_)
        return true;
    error = "Error: Command '" + name_ + "' expected " +
            std::to_string(expected_args_) + " arguments.";
    return false;
}

void ActionCommand::execute(const CliArguments &args, ICliOutputSink &sink) {
    handler_(args, sink);
}

} // namespace smart_home::cli
