#include "TestUtil.h"
#include "cli/ActionCommand.h"
#include "cli/CliProcessor.h"
#include "cli/DeviceCommand.h"
#include "cli/DeviceOutput.h"
#include "cli/GroupCommand.h"
#include "cli/ShowCommand.h"
#include "cli/StreamCliSink.h"
#include "cli/StringCliSink.h"
#include "descriptors/DeviceDescriptor.h"

#include <limits>
#include <sstream>

namespace {
using namespace smart_home;
using namespace smart_home::cli;

void expectLine(CliProcessor &cli, StringCliSink &sink, std::string_view line,
                CliStatus status, std::string_view output = {},
                std::string_view error = {}) {
    sink.clear();
    const std::string input(line);
    require(cli.processLine(line, sink) == status,
            "Wrong CLI status for: " + input);
    require(sink.output() == output,
            "Unexpected output for: " + input + "\n" + sink.output());
    require(sink.error() == error,
            "Unexpected error for: " + input + "\n" + sink.error());
}

class EchoCommand final : public ICliCommand {
  public:
    std::string_view name() const noexcept override { return "echo"; }

    bool validate(const CliArguments &args, std::string &error) const override {
        validated = args.size() == 2;
        if (!validated)
            error = "Expected two words.";
        return validated;
    }

    void execute(const CliArguments &args, ICliOutputSink &sink) override {
        require(validated, "Execute ran without successful validation");
        sink.writeLine(args[0]);
        sink.writeLine(args[1]);
        validated = false;
    }

  private:
    mutable bool validated = false;
};

void testProcessor() {
    CliProcessor cli;
    StringCliSink sink;
    cli.registerCommand(std::make_unique<EchoCommand>());
    expectException<std::invalid_argument>(
        [&] { cli.registerCommand(nullptr); });
    expectException<std::invalid_argument>(
        [&] { cli.registerCommand(std::make_unique<EchoCommand>()); });
    expectException<std::invalid_argument>([&] {
        cli.registerCommand(std::make_unique<ActionCommand>(
            "two words", 0, [](const CliArguments &, ICliOutputSink &) {}));
    });
    expectLine(cli, sink, {}, CliStatus::EmptyInput);
    expectLine(cli, sink, " \t\r\n\v\f", CliStatus::EmptyInput);
    expectLine(cli, sink, " bogus echo", CliStatus::CommandNotFound, {},
               "Error: Unknown command: bogus\n");
    expectLine(cli, sink, "1 brightness.get", CliStatus::CommandNotFound, {},
               "Error: Unknown command: 1\n");
    expectLine(cli, sink, "living_room light on_off.get",
               CliStatus::CommandNotFound, {},
               "Error: Unknown command: living_room\n");
    expectLine(cli, sink, "echo single", CliStatus::ValidationError, {},
               "Expected two words.\n");
    expectLine(cli, sink, " \techo\talpha  beta\r", CliStatus::Executed,
               "alpha\nbeta\n");
}

void testActions() {
    CliProcessor cli;
    StringCliSink sink;
    int calls = 0;
    for (const std::string name : {"scan", "save", "help", "list", "exit"}) {
        cli.registerCommand(std::make_unique<ActionCommand>(
            name, 0, [&, name](const CliArguments &args, ICliOutputSink &out) {
                require(args.empty(), "Action received unexpected arguments");
                ++calls;
                out.writeLine(name);
            }));
        expectLine(cli, sink, name, CliStatus::Executed, name + "\n");
        expectLine(cli, sink, name + " extra", CliStatus::ValidationError, {},
                   "Error: Command '" + name + "' expected 0 arguments.\n");
    }
    require(calls == 5, "Each action must run only once");
    expectException<std::invalid_argument>(
        [] { ActionCommand action("scan", 0, {}); });
    expectException<std::invalid_argument>([] {
        ActionCommand action("", 0,
                             [](const CliArguments &, ICliOutputSink &) {});
    });
}

constexpr auto invalid_id =
    "Error: Device ID must be a nonnegative integer in range.\n";
constexpr auto invalid_operation =
    "Error: Unknown operation or wrong argument count.\n";

void testShow() {
    CliProcessor cli;
    StringCliSink sink;
    int calls = 0;
    DeviceId last_id = -1;
    cli.registerCommand(
        std::make_unique<ShowCommand>([&](DeviceId id, ICliOutputSink &) {
            last_id = id;
            ++calls;
        }));
    expectLine(cli, sink, "show 0", CliStatus::Executed);
    require(last_id == 0, "Show must accept zero");
    expectLine(cli, sink, "show 9223372036854775807", CliStatus::Executed);
    require(last_id == std::numeric_limits<DeviceId>::max(),
            "Show truncated ID");
    for (auto invalid : {"show -1", "show 2x", "show 9223372036854775808"})
        expectLine(cli, sink, invalid, CliStatus::ValidationError, {},
                   invalid_id);
    for (auto invalid : {"show", "show 1 2"})
        expectLine(cli, sink, invalid, CliStatus::ValidationError, {},
                   "Error: Command 'show' expected 1 argument.\n");
    require(calls == 2, "Show ran for an invalid ID or argument count");
}

void testDeviceAndGroupOperations() {
    CliProcessor cli;
    StringCliSink sink;
    std::optional<DeviceOperation> last;
    DeviceId device_id = -1;
    std::string room;
    std::string category;
    cli.registerCommand(std::make_unique<DeviceCommand>(
        [&](DeviceId id, const DeviceOperation &op, ICliOutputSink &out) {
            device_id = id;
            last = op;
            if (id == 999)
                writeDeviceError(out, id, "Unknown device ID.");
        }));
    cli.registerCommand(std::make_unique<GroupCommand>(
        [&](std::string_view target_room, std::string_view target_category,
            const DeviceOperation &op, ICliOutputSink &) {
            room = target_room;
            category = target_category;
            last = op;
        }));

    const auto expectWrite = [&](std::string_view line, std::string_view op,
                                 Value value) {
        last.reset();
        expectLine(cli, sink, line, CliStatus::Executed);
        require(last && last->id == op && last->type == OperationType::Write &&
                    last->arguments == std::vector<Value>{value},
                "Wrong typed write argument for: " + std::string(line));
    };
    expectWrite("device 0 brightness.set 0", "brightness.set", 0);
    require(device_id == 0, "Device ID was not parsed");
    expectWrite("device 1 brightness.set 100", "brightness.set", 100);
    expectWrite("device 1 on_off.set true", "on_off.set", true);
    expectWrite("device 1 on_off.set false", "on_off.set", false);
    expectWrite("device 1 color.set 255 0 128", "color.set",
                Color{255, 0, 128});
    expectWrite("group study-2 light color.set 0 255 0", "color.set",
                Color{0, 255, 0});
    require(room == "study-2" && category == "light", "Wrong group selector");

    for (const std::string op :
         {"on_off.get", "brightness.get", "color.get", "temperature.get",
          "humidity.get", "contact.get", "battery.get", "power.get"}) {
        last.reset();
        expectLine(cli, sink, "device 1 " + op, CliStatus::Executed);
        require(last && last->id == op && last->type == OperationType::Read &&
                    last->arguments.empty(),
                "Wrong typed read operation for: " + op);
    }
    expectLine(cli, sink, "group ванная temperature_sensor temperature.get",
               CliStatus::Executed);
    require(room == "ванная" && category == "temperature_sensor" && last &&
                last->id == "temperature.get",
            "Wrong group read request");
    expectLine(cli, sink, "device 999 on_off.get", CliStatus::Executed, {},
               "Device 999 error: Unknown device ID.\n");
    require(device_id == 999,
            "Unknown device ID was not passed to the handler");

    last.reset();
    expectLine(cli, sink, "device 1", CliStatus::ValidationError, {},
               "Error: Expected device ID and operation.\n");
    expectLine(cli, sink, "group study light", CliStatus::ValidationError, {},
               "Error: Expected room, type and operation.\n");
    expectLine(cli, sink, "group study fan on_off.get",
               CliStatus::ValidationError, {}, "Error: Unknown device type.\n");
    constexpr char room_with_nul[] = "group stu\0dy light on_off.get";
    expectLine(cli, sink, {room_with_nul, sizeof(room_with_nul) - 1},
               CliStatus::ValidationError, {}, "Error: Invalid room ID.\n");
    for (auto invalid : {"device -1 on_off.get", "device 1x on_off.get",
                         "device 9223372036854775808 on_off.get"})
        expectLine(cli, sink, invalid, CliStatus::ValidationError, {},
                   invalid_id);

    for (const std::string prefix : {"device 1 ", "group study light "}) {
        for (auto invalid :
             {"bogus.get", "temperature.set 23", "on_off.get true",
              "on_off.set", "color.set 1 2", "color.set 1 2 3 4"})
            expectLine(cli, sink, prefix + invalid, CliStatus::ValidationError,
                       {}, invalid_operation);
        expectLine(cli, sink, prefix + "on_off.set TRUE",
                   CliStatus::ValidationError, {},
                   "Error: Boolean must be true or false.\n");
        for (auto invalid :
             {"brightness.set -1", "brightness.set 101", "brightness.set 2x",
              "brightness.set 999999999999999999999"})
            expectLine(cli, sink, prefix + invalid, CliStatus::ValidationError,
                       {}, "Error: Brightness must be an integer in 0..100.\n");
        for (auto invalid :
             {"color.set -1 0 0", "color.set 0 256 0", "color.set 0 0 2x"})
            expectLine(cli, sink, prefix + invalid, CliStatus::ValidationError,
                       {}, "Error: RGB component must be 0..255.\n");
    }
    require(!last, "An invalid operation reached the handler");
}

void testCategoryAndOutput() {
    const CapabilityDescriptor on_off("on_off", "", {});
    const CapabilityDescriptor brightness("brightness", "", {});
    const CapabilityDescriptor temperature("temperature", "", {});
    const CapabilityDescriptor color("color", "", {});
    require(deviceCategory(DeviceDescriptor(1, "", "", {on_off})) == "light",
            "Power capability must identify a light");
    require(deviceCategory(DeviceDescriptor(
                1, "", "", {temperature, brightness})) == "light",
            "Brightness must identify a light even when temperature is also "
            "available");
    require(deviceCategory(DeviceDescriptor(2, "", "", {temperature})) ==
                "temperature_sensor",
            "Temperature capability must identify a sensor");
    require(deviceCategory(DeviceDescriptor(3, "", "", {color})) == "device",
            "Other capabilities must use the general category");

    std::ostringstream out;
    std::ostringstream err;
    StreamCliSink sink(out, err);
    writeDeviceSummary(sink, 1, "Light", "light", "living_room");
    writeDeviceProperty(sink, "on_off", Value{false});
    writeDeviceResult(sink, 1, "on_off.set", std::nullopt);
    writeDeviceResult(sink, 1, "brightness.get", Value{75});
    writeDeviceResult(sink, 1, "color.get", Value{Color{255, 128, 0}});
    writeDeviceError(sink, 2, "Capability not supported");
    require(out.str() == "Device 1 Light light room=living_room\n"
                         "  on_off = false\n"
                         "Device 1 on_off.set: applied\n"
                         "Device 1 brightness.get: 75\n"
                         "Device 1 color.get: 255 128 0\n",
            "Unexpected device result formatting");
    require(err.str() == "Device 2 error: Capability not supported\n",
            "Device error must use the stream for diagnostics");
}

} // namespace

int main() {
    testProcessor();
    testActions();
    testShow();
    testDeviceAndGroupOperations();
    testCategoryAndOutput();
}
