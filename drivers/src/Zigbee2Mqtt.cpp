#include "drivers/Zigbee2Mqtt.h"

#include "drivers/CapabilityCatalog.h"
#include "drivers/UniversalDeviceDriver.h"
#include "mqtt/interfaces/IMqttClient.h"

#include <json-c/json.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace smart_home {
namespace {

constexpr int zigbee_brightness_max = 254;

std::string stateTopic(const std::string &physical_id) {
    return "zigbee2mqtt/" + physical_id;
}

std::string commandPayload(const std::string &id, const Value &value) {
    if (id == "on_off")
        return std::get<bool>(value) ? R"({"state":"ON"})"
                                     : R"({"state":"OFF"})";
    if (id == "brightness") {
        const auto level =
            (std::get<int>(value) * zigbee_brightness_max + 50) / 100;
        return R"({"brightness":)" + std::to_string(level) + "}";
    }
    if (id == "color") {
        const auto color = std::get<Color>(value);
        return R"({"color":{"r":)" + std::to_string(color.red) + R"(,"g":)" +
               std::to_string(color.green) + R"(,"b":)" +
               std::to_string(color.blue) + "}}";
    }
    throw std::logic_error("No Zigbee2MQTT encoding for capability: " + id);
}

using Json = std::unique_ptr<json_object, decltype(&json_object_put)>;

Json parse(const std::vector<std::uint8_t> &payload) {
    Json root(nullptr, json_object_put);
    if (payload.empty() || payload.size() > std::numeric_limits<int>::max())
        return root;
    const std::unique_ptr<json_tokener, decltype(&json_tokener_free)> parser(
        json_tokener_new(), json_tokener_free);
    if (!parser)
        throw std::bad_alloc();
    json_tokener_set_flags(parser.get(),
                           JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    root.reset(json_tokener_parse_ex(
        parser.get(), reinterpret_cast<const char *>(payload.data()),
        static_cast<int>(payload.size())));
    if (json_tokener_get_error(parser.get()) != json_tokener_success ||
        json_tokener_get_parse_end(parser.get()) != payload.size() ||
        !json_object_is_type(root.get(), json_type_object))
        root.reset();
    return root;
}

json_object *field(json_object *object, const char *key) {
    json_object *result = nullptr;
    json_object_object_get_ex(object, key, &result);
    return result;
}

std::string_view text(json_object *value) {
    if (!json_object_is_type(value, json_type_string))
        return {};
    return {json_object_get_string(value),
            static_cast<std::size_t>(json_object_get_string_len(value))};
}

std::optional<double> number(json_object *value) {
    if (!json_object_is_type(value, json_type_double) &&
        !json_object_is_type(value, json_type_int))
        return std::nullopt;
    const auto parsed = json_object_get_double(value);
    if (!std::isfinite(parsed))
        return std::nullopt;
    return parsed;
}

std::optional<int> integer(json_object *value, int maximum) {
    if (!json_object_is_type(value, json_type_int))
        return std::nullopt;
    const auto number = json_object_get_int64(value);
    if (number < 0 || number > maximum)
        return std::nullopt;
    return static_cast<int>(number);
}

std::uint8_t channel(double value) {
    return static_cast<std::uint8_t>(
        std::lround(255 * std::clamp(value, 0.0, 1.0)));
}

Color fromChromaticity(double x, double y) {
    const double z = 1.0 - x - y;
    // CIE XYZ to linear sRGB. Normalizing retains chromaticity at full
    // brightness.
    const auto red = 3.2406 * x - 1.5372 * y - 0.4986 * z;
    const auto green = -0.9689 * x + 1.8758 * y + 0.0415 * z;
    const auto blue = 0.0557 * x - 0.2040 * y + 1.0570 * z;
    const auto scale = std::max({red, green, blue});
    const auto srgb = [scale](double linear) {
        const auto level = std::max(linear / scale, 0.0);
        return channel(level <= 0.0031308
                           ? 12.92 * level
                           : 1.055 * std::pow(level, 1.0 / 2.4) - 0.055);
    };
    return {srgb(red), srgb(green), srgb(blue)};
}

Color fromHueSaturation(double hue, double saturation) {
    const auto sector = std::fmod(hue, 360.0) / 60.0;
    const auto chroma = saturation / 100.0;
    const auto secondary =
        chroma * (1.0 - std::abs(std::fmod(sector, 2.0) - 1.0));
    const auto floor = 1.0 - chroma;
    const std::array<std::array<double, 3>, 6> components{
        {{chroma, secondary, 0.0},
         {secondary, chroma, 0.0},
         {0.0, chroma, secondary},
         {0.0, secondary, chroma},
         {secondary, 0.0, chroma},
         {chroma, 0.0, secondary}}};
    const auto &rgb = components[static_cast<std::size_t>(sector)];
    return {channel(rgb[0] + floor), channel(rgb[1] + floor),
            channel(rgb[2] + floor)};
}

std::optional<Value> decodeColor(json_object *value, std::string_view mode) {
    if (!json_object_is_type(value, json_type_object))
        return std::nullopt;
    const auto r = integer(field(value, "r"), 255);
    const auto g = integer(field(value, "g"), 255);
    const auto b = integer(field(value, "b"), 255);
    if (r && g && b)
        return Color{static_cast<std::uint8_t>(*r),
                     static_cast<std::uint8_t>(*g),
                     static_cast<std::uint8_t>(*b)};
    const auto x = number(field(value, "x"));
    const auto y = number(field(value, "y"));
    if (mode != "hs" && x && y && *x >= 0 && *y > 0 && *x + *y <= 1)
        return fromChromaticity(*x, *y);
    const auto hue = number(field(value, "hue"));
    const auto saturation = number(field(value, "saturation"));
    if (mode != "xy" && hue && saturation && *hue >= 0 && *hue <= 360 &&
        *saturation >= 0 && *saturation <= 100)
        return fromHueSaturation(*hue, *saturation);
    return std::nullopt;
}

std::optional<Value> decode(const std::string &id, json_object *root) {
    if (id == "on_off") {
        const auto state = text(field(root, "state"));
        if (state == "ON" || state == "OFF")
            return state == "ON";
    } else if (id == "brightness") {
        if (const auto level = integer(field(root, "brightness"), 255))
            return (std::min(*level, zigbee_brightness_max) * 100 +
                    zigbee_brightness_max / 2) /
                   zigbee_brightness_max;
    } else if (id == "color") {
        return decodeColor(field(root, "color"),
                           text(field(root, "color_mode")));
    } else if (id == "contact") {
        auto *value = field(root, "contact");
        if (json_object_is_type(value, json_type_boolean))
            return !json_object_get_boolean(value);
    } else if (id == "battery") {
        if (const auto level = number(field(root, "battery"));
            level && *level >= 0 && *level <= 100)
            return static_cast<int>(std::lround(*level));
    } else if (const auto measure = number(field(root, id.c_str()))) {
        return *measure;
    }
    return std::nullopt;
}

} // namespace

Zigbee2Mqtt::Zigbee2Mqtt(mqtt::IMqttClient &client) noexcept
    : client_(client) {}

DeviceCreator Zigbee2Mqtt::creator() {
    return [this](const DeviceRecord &record) {
        if (record.driver_id != "zigbee")
            throw std::invalid_argument(
                "Zigbee creator requires the zigbee protocol");
        const auto &id = record.physical_id;
        if (id.empty() || id.find_first_of("+#") != std::string::npos ||
            id.find('\0') != std::string::npos || id == "bridge" ||
            id.compare(0, 7, "bridge/") == 0 || id.front() == '/' ||
            id.back() == '/')
            throw std::invalid_argument("Invalid Zigbee2MQTT device topic");
        return std::make_unique<UniversalDeviceDriver>(
            record, [this, topic = stateTopic(id) + "/set"](
                        const std::string &capability_id, const Value &value) {
                publish(topic, capability_id, value);
            });
    };
}

void Zigbee2Mqtt::publish(const std::string &topic,
                          const std::string &capability_id,
                          const Value &value) {
    pending_.remove_if([&](mqtt::MqttDeliveryToken token) {
        return client_
                   .waitForOperation(token, std::chrono::milliseconds::zero())
                   .status != mqtt::MqttOperationStatus::Pending;
    });
    const auto payload = commandPayload(capability_id, value);
    const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
    auto &token = pending_.emplace_back();
    try {
        token = client_.publish(topic, bytes, mqtt::MqttQoS::AtLeastOnce);
    } catch (...) {
        pending_.pop_back();
        throw;
    }
}

bool Zigbee2Mqtt::applyTelemetry(IDeviceDriver &driver,
                                 const mqtt::MqttMessage &message) const {
    if (message.topic != stateTopic(driver.descriptor().getPhysicalId()))
        return false;
    const auto root = parse(message.payload);
    if (!root)
        return false;
    bool accepted = false;
    for (const auto &capability : driver.descriptor().getCapabilities()) {
        const auto &id = capability.getId();
        const auto *entry = CapabilityCatalog::find(id);
        if (!entry)
            continue;
        if (const auto value = decode(id, root.get());
            value && entry->accepts(*value)) {
            entry->store(driver.device(), *value);
            accepted = true;
        }
    }
    return accepted;
}

} // namespace smart_home
