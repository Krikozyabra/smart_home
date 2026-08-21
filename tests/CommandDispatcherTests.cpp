#include "TestUtil.h"
#include "command/Value.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/SimulatedTemperatureSensorDriver.h"
#include "execution/CommandDispatcher.h"

#include <stdexcept>

int main() {
    smart_home::DeviceRegistry registry;

    smart_home::CommandDispatcher command_dispatcher(registry);

    registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
        0, "AC:12:BD", "Light1", false, 50, Color{100, 50, 200}));
    registry.add(std::make_unique<smart_home::SimulatedTemperatureSensorDriver>(
        1, "D1:A0:67", "TemperatureSensor in bath", 23.0));

    smart_home::Command color_get(0, "color.get", {});
    smart_home::Command color_get_with_arg(
        0, "color.get", std::vector<smart_home::Value>{Color{22, 33, 44}});
    smart_home::Command color_set(
        0, "color.set", std::vector<smart_home::Value>{Color{10, 20, 30}});
    smart_home::Command color_set_without_args(
        0, "color.set", std::vector<smart_home::Value>{});
    smart_home::Command color_set_with_int(0, "color.set",
                                           std::vector<smart_home::Value>{25});
    smart_home::Command color_set_for_temperature_sensor(
        1, "color.set", std::vector<smart_home::Value>{Color{30, 40, 50}});

    smart_home::Command on_off_get(0, "on_off.get", {});
    smart_home::Command on_off_get_with_arg(
        0, "on_off.get", std::vector<smart_home::Value>{true});

    smart_home::Command on_off_set(0, "on_off.set",
                                   std::vector<smart_home::Value>{true});
    smart_home::Command on_off_set_with_int(0, "on_off.set",
                                            std::vector<smart_home::Value>{1});
    smart_home::Command on_off_set_without_arg(0, "on_off.set", {});
    smart_home::Command on_off_set_for_temperature_sensor(
        1, "on_off.set", std::vector<smart_home::Value>{true});

    smart_home::Command brightness_set(0, "brightness.set",
                                       std::vector<smart_home::Value>{75});
    smart_home::Command brightness_set_no_args(
        0, "brightness.set", std::vector<smart_home::Value>{});
    smart_home::Command brightness_set_with_double(
        0, "brightness.set", std::vector<smart_home::Value>{75.0});
    smart_home::Command brightness_set_with_low(
        0, "brightness.set", std::vector<smart_home::Value>{-1});
    smart_home::Command brightness_set_with_high(
        0, "brightness.set", std::vector<smart_home::Value>{101});
    smart_home::Command brightness_set_with_broken_id(
        999, "brightness.set", std::vector<smart_home::Value>{30});
    smart_home::Command brightness_set_for_temperature_sensor(
        1, "brightness.set", std::vector<smart_home::Value>{45});

    smart_home::Command brightness_get(0, "brightness.get", {});
    smart_home::Command brightness_get_with_arg(
        0, "brightness.get", std::vector<smart_home::Value>{30});

    smart_home::Command temperature_get(1, "temperature.get",
                                        std::vector<smart_home::Value>{});

    std::optional<smart_home::Value> result =
        command_dispatcher.execute(temperature_get);

    require(result.has_value(), "temperature.get must return a value");
    require(std::holds_alternative<double>(*result),
            "temperature.get must return double value");
    require(std::get<double>(*result) == 23.0,
            "temperature.get must return a correct device's value");

    // Tests for command creation
    require(brightness_set.getDeviceId() == 0,
            "brightness.set command class must have device_id = 0");
    require(brightness_set.getArguments().size() == 1,
            "brightness.set command class must have exactly one argument");
    require(
        brightness_set.getOperationId() == "brightness.set",
        "brightness_set command class must have command_id='brightness.set'");
    require(std::holds_alternative<int>(brightness_set.getArguments()[0]),
            "brightness.set command class must have an integer argument");
    require(std::get<int>(brightness_set.getArguments()[0]) == 75,
            "brightness.set command class must have a value of the argument "
            "equal 75");
    require(temperature_get.getArguments().size() == 0,
            "temperature.get command class must have 0 arguments");

    // Test for bad execution of brightness.get
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(brightness_get_with_arg); });

    // Tests for bad execution of brightness.set
    expectException<std::runtime_error>(
        [&] { command_dispatcher.execute(brightness_set_with_broken_id); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(brightness_set_no_args); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(brightness_set_with_double); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(brightness_set_with_high); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(brightness_set_with_low); });
    expectException<std::invalid_argument>([&] {
        command_dispatcher.execute(brightness_set_for_temperature_sensor);
    });

    result = command_dispatcher.execute(brightness_set);
    require(!result.has_value(), "brightness.set must not return a value");

    result = command_dispatcher.execute(brightness_get);

    // Tests for execution of brightness.get
    require(result.has_value(), "brightness.get must return a value");
    require(std::holds_alternative<int>(*result),
            "brightness.get must return an integer value");
    require(std::get<int>(*result) == 75, "brightness.get must return 75");

    // Tests for execution on_off.get
    result = command_dispatcher.execute(on_off_get);
    require(result.has_value(), "on_off.get must return a value");
    require(std::holds_alternative<bool>(*result),
            "on_off.get must return a boolean value");
    require(std::get<bool>(*result) == false,
            "on_off.get must return false value");

    // Tests for bad execution on_off.get
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(on_off_get_with_arg); });

    // Tests for execution on_off.set
    result = command_dispatcher.execute(on_off_set);
    require(!result.has_value(), "on_off.set must not return a value");
    result = command_dispatcher.execute(on_off_get);
    require(std::get<bool>(*result) == true,
            "on_off.set must set true for power parameter");

    // Tests for bad execution on_off.set
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(on_off_set_without_arg); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(on_off_set_with_int); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(on_off_set_for_temperature_sensor); });

    // Tests for execution color.get
    result = command_dispatcher.execute(color_get);
    require(result.has_value(), "color.get must return a value");
    require(std::holds_alternative<Color>(*result),
            "color.get must return a Color object");
    require(std::get<Color>(*result) == Color{100, 50, 200},
            "color.get must return Color{100, 50, 200}");

    // Tests for bad execution color.get
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(color_get_with_arg); });

    // Tests for execution color.set
    result = command_dispatcher.execute(color_set);
    require(!result.has_value(), "color.set must not return value");
    result = command_dispatcher.execute(color_get);
    require(result.has_value(), "color.get must return a value");
    require(std::holds_alternative<Color>(*result),
            "color.get must return Color");
    require(std::get<Color>(*result) == Color{10, 20, 30},
            "color.set must change the color");

    // Tests for bad execution color.set
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(color_set_without_args); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(color_set_with_int); });
    expectException<std::invalid_argument>(
        [&] { command_dispatcher.execute(color_set_for_temperature_sensor); });
    return 0;
}
