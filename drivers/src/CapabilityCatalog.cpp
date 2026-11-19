#include "drivers/CapabilityCatalog.h"

#include "descriptors/OperationDescriptor.h"
#include "descriptors/ParameterDescriptor.h"
#include "devices/PropertyTags.h"

#include <optional>
#include <string>
#include <utility>

namespace smart_home {
namespace {

CapabilityDescriptor readWriteCapability(const std::string &id,
                                         std::string name, std::string set_name,
                                         std::string get_name,
                                         const ParameterDescriptor &parameter) {
    return {id,
            std::move(name),
            {{id + ".set",
              std::move(set_name),
              OperationType::Write,
              {parameter},
              std::nullopt},
             {id + ".get",
              std::move(get_name),
              OperationType::Read,
              {},
              parameter}}};
}

CapabilityDescriptor readOnlyCapability(const std::string &id, std::string name,
                                        std::string get_name,
                                        const ParameterDescriptor &parameter) {
    return {id,
            std::move(name),
            {{id + ".get",
              std::move(get_name),
              OperationType::Read,
              {},
              parameter}}};
}

IntegerMetadata metadata(const PropertyTag<int> &property) {
    return {property.min_value, property.max_value, property.unit};
}

FloatMetadata metadata(const PropertyTag<double> &property) {
    return {property.min_value, property.max_value, property.unit};
}

} // namespace

const CapabilityCatalog::Entry<bool> &CapabilityCatalog::onOff() {
    static const Entry<bool> entry{
        props::light::State,
        readWriteCapability("on_off", "Work with power of the device",
                            "Set the device's power",
                            "Get the device's power status",
                            {"on_off.parameter.bool", "Power",
                             ValueType::Boolean, BooleanMetadata{}})};
    return entry;
}

const CapabilityCatalog::Entry<int> &CapabilityCatalog::brightness() {
    static const Entry<int> entry{
        props::light::Brightness,
        readWriteCapability(
            "brightness", "Work with device's brightness",
            "Set the device's brightness", "Get the device's brightness",
            {"brightness.parameter.integer", "Brightness", ValueType::Integer,
             metadata(props::light::Brightness)})};
    return entry;
}

const CapabilityCatalog::Entry<Color> &CapabilityCatalog::color() {
    static const Entry<Color> entry{
        props::light::Rgb,
        readWriteCapability("color", "Work with device's color",
                            "Set the device's color", "Get the device's color",
                            {"color.parameter.color", "Color", ValueType::Color,
                             ColorMetadata{"RGB"}})};
    return entry;
}

const CapabilityCatalog::Entry<double> &CapabilityCatalog::temperature() {
    static const Entry<double> entry{
        props::climate::Temperature,
        readOnlyCapability("temperature", "Work with temperature of the device",
                           "Get the device's temperature",
                           {"temperature.parameter.float", "Temperature",
                            ValueType::Float,
                            metadata(props::climate::Temperature)})};
    return entry;
}

const CapabilityCatalog::Entry<double> &CapabilityCatalog::humidity() {
    static const Entry<double> entry{
        props::climate::Humidity,
        readOnlyCapability("humidity", "Humidity", "Get the device's humidity",
                           {"humidity.parameter.float", "Humidity",
                            ValueType::Float,
                            metadata(props::climate::Humidity)})};
    return entry;
}

const CapabilityCatalog::Entry<bool> &CapabilityCatalog::contact() {
    static const Entry<bool> entry{
        props::security::Contact,
        readOnlyCapability("contact", "Contact", "Get the contact's state",
                           {"contact.parameter.bool", "Contact",
                            ValueType::Boolean, BooleanMetadata{}})};
    return entry;
}

const CapabilityCatalog::Entry<int> &CapabilityCatalog::battery() {
    static const Entry<int> entry{
        props::security::Battery,
        readOnlyCapability(
            "battery", "Battery", "Get the device's battery level",
            {"battery.parameter.integer", "Battery", ValueType::Integer,
             metadata(props::security::Battery)})};
    return entry;
}

const CapabilityCatalog::Entry<double> &CapabilityCatalog::power() {
    static const Entry<double> entry{
        props::power::PowerWatts,
        readOnlyCapability(
            "power", "Active power", "Get the device's active power",
            {"power.parameter.float", "Active power", ValueType::Float,
             metadata(props::power::PowerWatts)})};
    return entry;
}

} // namespace smart_home
