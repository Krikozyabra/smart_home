#include "TestUtil.h"
#include "devices/Device.h"
#include "devices/PropertyTags.h"
#include "devices/views/BrightnessView.h"
#include "devices/views/ColorView.h"
#include "devices/views/OnOffView.h"
#include "devices/views/TemperatureView.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
using namespace smart_home;

constexpr PropertyTag<std::string> Label{"custom.label"};
constexpr PropertyTag<char> Grade{"custom.grade", 'A', 'F'};

static_assert(!std::is_copy_constructible_v<Device>);
static_assert(!std::is_copy_assignable_v<Device>);
static_assert(std::is_nothrow_move_constructible_v<Device>);
static_assert(std::is_nothrow_move_assignable_v<Device>);
static_assert(!std::is_constructible_v<TemperatureView, Device &&>);

static_assert(std::string_view{Label.key} == "custom.label");
static_assert(std::is_same_v<decltype(Label)::Type, std::string>);
static_assert(Grade.validate('A') && Grade.validate('F') &&
              !Grade.validate('G'));
static_assert(props::light::State.validate(false));
static_assert(props::light::Rgb.validate(Color{0, 128, 255}));
static_assert(props::light::Brightness.has_min &&
              props::light::Brightness.has_max &&
              props::light::Brightness.min_value == 0 &&
              props::light::Brightness.max_value == 100 &&
              std::string_view{props::light::Brightness.unit} == "%");
static_assert(props::climate::Temperature.validate(-50.0) &&
              props::climate::Temperature.validate(100.0) &&
              !props::climate::Temperature.validate(-50.1) &&
              !props::climate::Temperature.validate(100.1) &&
              std::string_view{props::climate::Temperature.unit} == "°C");

void identityAndMoves() {
    Device device(17, "Lamp");
    require(device.getId() == 17 && device.getName() == "Lamp" &&
                device.getProperties().empty(),
            "Construction must set identity and start with no properties");
    device.setName("Desk lamp");
    expectException<std::invalid_argument>([&] { device.setName(""); });
    require(device.getName() == "Desk lamp",
            "An invalid rename must preserve the previous name");
    device.set(props::light::State, true);

    Device moved(std::move(device));
    require(moved.getId() == 17 && moved.getName() == "Desk lamp" &&
                moved.get(props::light::State) == true,
            "Move construction must transfer identity and properties");
    Device assigned(18, "Other");
    assigned.set(Grade, 'C');
    assigned = std::move(moved);
    require(assigned.getId() == 17 && assigned.getName() == "Desk lamp" &&
                assigned.get(props::light::State) == true &&
                !assigned.has(Grade),
            "Move assignment must replace identity and properties");
}

void rawProperties() {
    Device device(1, "Device");
    require(!device.getProperty("reading") && !device.hasProperty("reading") &&
                !device.removeProperty("reading"),
            "Missing properties must be reported by raw operations");
    device.setProperty("reading", 14);
    require(device.hasProperty("reading") &&
                device.getProperty("reading") == Value{14},
            "Raw writes must be observable in the dictionary");
    device.setProperty("reading", 2.5);
    require(device.getProperty("reading") == Value{2.5} &&
                device.getProperties().size() == 1 &&
                device.getProperties().at("reading") == Value{2.5},
            "Raw writes must replace the old value and type");

    expectException<std::invalid_argument>([&] { device.setProperty("", 1); });
    expectException<std::invalid_argument>([&] { device.getProperty(""); });
    expectException<std::invalid_argument>([&] { device.hasProperty(""); });
    expectException<std::invalid_argument>([&] { device.removeProperty(""); });
    require(device.removeProperty("reading") && device.getProperties().empty(),
            "Removing the stored property must empty the dictionary");
}

void typedProperties() {
    Device device(1, "Device");
    constexpr auto brightness = props::light::Brightness;
    require(!device.get(brightness) && !device.has(brightness),
            "An unregistered tag must report no property");
    device.setProperty(brightness.key, 50.0);
    require(device.has(brightness) && !device.get(brightness),
            "Typed reads must reject another type without hiding the key");

    device.set(brightness, 0);
    require(device.get(brightness) == 0, "The lower bound must be accepted");
    device.set(brightness, 100);
    require(device.get(brightness) == 100, "The upper bound must be accepted");
    expectException<std::invalid_argument>([&] { device.set(brightness, -1); });
    expectException<std::invalid_argument>(
        [&] { device.set(brightness, 101); });
    require(device.get(brightness) == 100,
            "Rejected writes must preserve the previous value");

    constexpr auto temperature = props::climate::Temperature;
    expectException<std::invalid_argument>([&] {
        device.set(temperature, std::numeric_limits<double>::quiet_NaN());
    });
    expectException<std::invalid_argument>([&] {
        device.set(temperature, std::numeric_limits<double>::infinity());
    });
    require(!device.has(temperature), "Rejected values must not create a key");

    const std::string label = "Desk lamp beside the window";
    device.set(Label, label);
    device.set(Grade, 'E');
    require(device.get(Label) == label && device.get(Grade) == 'E',
            "String and character values must round-trip through their tags");
}

void views() {
    Device device(1, "Climate and lighting");
    auto power_view = device.view<OnOffView>();
    BrightnessView brightness_view(device);
    auto color_view = device.view<ColorView>();
    auto temperature_view = std::as_const(device).view<TemperatureView>();
    IOnOff &power = power_view;
    IBrightness &brightness = brightness_view;
    IColor &color = color_view;
    const ITemperature &temperature = temperature_view;

    expectException<std::logic_error>([&] { power.isOn(); });
    expectException<std::logic_error>([&] { brightness.getBrightness(); });
    expectException<std::logic_error>([&] { color.getColor(); });
    expectException<std::logic_error>([&] { temperature.getTemperature(); });

    power.setOn(false);
    brightness.setBrightness(73);
    color.setColor(Color{0, 128, 255});
    device.set(props::climate::Temperature, 24.5);
    require(!power.isOn() && device.get(props::light::State) == false &&
                brightness.getBrightness() == 73 &&
                device.get(props::light::Brightness) == 73 &&
                color.getColor() == Color{0, 128, 255} &&
                device.get(props::light::Rgb) == Color{0, 128, 255} &&
                temperature.getTemperature() == 24.5,
            "Views must read and write the underlying properties");
    expectException<std::invalid_argument>(
        [&] { brightness.setBrightness(101); });
    require(brightness.getBrightness() == 73,
            "Rejected brightness must preserve the last value");

    device.setProperty(props::light::Brightness.key, 256);
    expectException<std::logic_error>([&] { brightness.getBrightness(); });
    device.setProperty(props::light::State.key, 1);
    expectException<std::logic_error>([&] { power.isOn(); });
}

} // namespace

int main() {
    identityAndMoves();
    rawProperties();
    typedProperties();
    views();
}
