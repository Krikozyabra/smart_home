#include "cli/DeviceCommand.h"
#include "cli/GroupCommand.h"
#include "cli/ShowCommand.h"
#include "drivers/CapabilityCatalog.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace smart_home::cli {
namespace {

constexpr auto invalid_operation =
    "Error: Unknown operation or wrong argument count.";

template <typename T> std::optional<T> parseInteger(std::string_view text) {
    if (text.empty())
        return std::nullopt;
    T value{};
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        return std::nullopt;
    return value;
}

std::optional<DeviceId> parseId(std::string_view text, std::string &error) {
    const auto id = parseInteger<DeviceId>(text);
    if (id && *id >= 0)
        return id;
    error = "Error: Device ID must be a nonnegative integer in range.";
    return std::nullopt;
}

std::optional<DeviceId> parseShowId(const CliArguments &args,
                                    std::string &error) {
    if (args.size() == 1)
        return parseId(args.front(), error);
    error = "Error: Command 'show' expected 1 argument.";
    return std::nullopt;
}

const OperationDescriptor *findOperation(std::string_view id) {
    const auto separator = id.rfind('.');
    if (separator == std::string_view::npos)
        return nullptr;
    const auto *capability = CapabilityCatalog::find(id.substr(0, separator));
    if (!capability)
        return nullptr;
    const auto &operations = capability->descriptor.getOperations();
    const auto found = std::find_if(
        operations.begin(), operations.end(),
        [&](const auto &operation) { return operation.getId() == id; });
    return found == operations.end() ? nullptr : &*found;
}

std::optional<Value> parseWriteValue(const ParameterDescriptor &parameter,
                                     const CliArguments &args,
                                     std::size_t first, std::string &error) {
    switch (parameter.getValueType()) {
    case ValueType::Boolean:
        if (args[first] == "true" || args[first] == "false")
            return Value{args[first] == "true"};
        error = "Error: Boolean must be true or false.";
        return std::nullopt;
    case ValueType::Integer: {
        const auto &range = std::get<IntegerMetadata>(parameter.getMetadata());
        const auto number = parseInteger<int>(args[first]);
        if (number && *number >= range.min && *number <= range.max)
            return Value{*number};
        error = "Error: " + parameter.name + " must be an integer in " +
                std::to_string(range.min) + ".." + std::to_string(range.max) +
                ".";
        return std::nullopt;
    }
    case ValueType::Color: {
        if (std::get<ColorMetadata>(parameter.getMetadata()).color_model !=
            "RGB")
            break;
        std::array<std::uint8_t, 3> rgb{};
        for (std::size_t i = 0; i < rgb.size(); ++i) {
            const auto component = parseInteger<int>(args[first + i]);
            if (!component || *component < 0 || *component > 255) {
                error = "Error: RGB component must be 0..255.";
                return std::nullopt;
            }
            rgb[i] = static_cast<std::uint8_t>(*component);
        }
        return Value{Color{rgb[0], rgb[1], rgb[2]}};
    }
    case ValueType::Float:
    case ValueType::String:
        break;
    }
    error = invalid_operation;
    return std::nullopt;
}

std::optional<DeviceOperation> parseOperation(const CliArguments &args,
                                              std::size_t index,
                                              std::string &error) {
    const auto *descriptor = findOperation(args[index]);
    if (!descriptor) {
        error = invalid_operation;
        return std::nullopt;
    }

    const auto count = args.size() - index - 1;
    const auto type = descriptor->getType();
    const auto &parameters = descriptor->getInputParameters();
    if (type == OperationType::Read && count == 0)
        return DeviceOperation{descriptor->getId(), type, {}};
    if (type == OperationType::Write && parameters.size() == 1) {
        const auto &parameter = parameters.front();
        const auto expected =
            parameter.getValueType() == ValueType::Color ? 3u : 1u;
        if (count == expected) {
            auto value = parseWriteValue(parameter, args, index + 1, error);
            if (!value)
                return std::nullopt;
            return DeviceOperation{
                descriptor->getId(), type, {std::move(*value)}};
        }
    }
    error = invalid_operation;
    return std::nullopt;
}

struct DeviceRequest {
    DeviceId id;
    DeviceOperation operation;
};

std::optional<DeviceRequest> parseDeviceRequest(const CliArguments &args,
                                                std::string &error) {
    if (args.size() < 2) {
        error = "Error: Expected device ID and operation.";
        return std::nullopt;
    }
    const auto id = parseId(args[0], error);
    if (!id)
        return std::nullopt;
    if (auto operation = parseOperation(args, 1, error))
        return DeviceRequest{*id, std::move(*operation)};
    return std::nullopt;
}

std::optional<DeviceOperation> parseGroupOperation(const CliArguments &args,
                                                   std::string &error) {
    if (args.size() < 3) {
        error = "Error: Expected room, type and operation.";
        return std::nullopt;
    }
    if (args[0].empty() || args[0].find('\0') != std::string_view::npos) {
        error = "Error: Invalid room ID.";
        return std::nullopt;
    }
    if (args[1] != "light" && args[1] != "temperature_sensor") {
        error = "Error: Unknown device type.";
        return std::nullopt;
    }
    return parseOperation(args, 2, error);
}

} // namespace

ShowCommand::ShowCommand(Handler handler) : handler_(std::move(handler)) {
    if (!handler_)
        throw std::invalid_argument("Command handler cannot be null.");
}

std::string_view ShowCommand::name() const noexcept { return "show"; }

bool ShowCommand::validate(const CliArguments &args, std::string &error) const {
    return parseShowId(args, error).has_value();
}

void ShowCommand::execute(const CliArguments &args, ICliOutputSink &sink) {
    std::string error;
    if (const auto id = parseShowId(args, error))
        handler_(*id, sink);
    else
        sink.writeError(error);
}

DeviceCommand::DeviceCommand(Handler handler) : handler_(std::move(handler)) {
    if (!handler_)
        throw std::invalid_argument("Command handler cannot be null.");
}

std::string_view DeviceCommand::name() const noexcept { return "device"; }

bool DeviceCommand::validate(const CliArguments &args,
                             std::string &error) const {
    return parseDeviceRequest(args, error).has_value();
}

void DeviceCommand::execute(const CliArguments &args, ICliOutputSink &sink) {
    std::string error;
    if (const auto request = parseDeviceRequest(args, error))
        handler_(request->id, request->operation, sink);
    else
        sink.writeError(error);
}

GroupCommand::GroupCommand(Handler handler) : handler_(std::move(handler)) {
    if (!handler_)
        throw std::invalid_argument("Command handler cannot be null.");
}

std::string_view GroupCommand::name() const noexcept { return "group"; }

bool GroupCommand::validate(const CliArguments &args,
                            std::string &error) const {
    return parseGroupOperation(args, error).has_value();
}

void GroupCommand::execute(const CliArguments &args, ICliOutputSink &sink) {
    std::string error;
    if (const auto operation = parseGroupOperation(args, error))
        handler_(args[0], args[1], *operation, sink);
    else
        sink.writeError(error);
}

} // namespace smart_home::cli
