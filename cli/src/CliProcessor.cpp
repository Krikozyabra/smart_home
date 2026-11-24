#include "cli/CliProcessor.h"

#include <stdexcept>
#include <utility>

namespace smart_home::cli {
namespace {

constexpr std::string_view whitespace = " \t\n\r\f\v";

std::string_view nextToken(std::string_view &input) {
    const auto start = input.find_first_not_of(whitespace);
    if (start == std::string_view::npos)
        return {};
    input.remove_prefix(start);
    const auto token = input.substr(0, input.find_first_of(whitespace));
    input.remove_prefix(token.size());
    return token;
}

} // namespace

void CliProcessor::registerCommand(std::unique_ptr<ICliCommand> command) {
    if (!command)
        throw std::invalid_argument("Cannot register null command.");

    const std::string name(command->name());
    if (name.empty() || name.find_first_of(whitespace) != std::string::npos)
        throw std::invalid_argument("Command name must be one nonempty token.");
    if (!commands_.try_emplace(name, std::move(command)).second)
        throw std::invalid_argument("Command already registered: " + name);
}

CliStatus CliProcessor::processLine(std::string_view line,
                                    ICliOutputSink &sink) {
    const auto token = nextToken(line);
    if (token.empty())
        return CliStatus::EmptyInput;

    const std::string name(token);
    const auto found = commands_.find(name);
    if (found == commands_.end()) {
        sink.writeError("Error: Unknown command: " + name);
        return CliStatus::CommandNotFound;
    }

    CliArguments args;
    for (auto arg = nextToken(line); !arg.empty(); arg = nextToken(line))
        args.push_back(arg);

    auto &command = *found->second;
    std::string error;
    if (!command.validate(args, error)) {
        sink.writeError(error);
        return CliStatus::ValidationError;
    }
    command.execute(args, sink);
    return CliStatus::Executed;
}

} // namespace smart_home::cli
