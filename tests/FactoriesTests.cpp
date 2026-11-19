#include "TestUtil.h"
#include "devices/PropertyTags.h"
#include "drivers/CapabilityCatalog.h"
#include "drivers/DeviceCreator.h"
#include "drivers/UniversalDeviceDriver.h"

#include <stdexcept>

int main() {
    using namespace smart_home;
    const DeviceRecord record{1, "SIM-1", "simulated", "Lamp and sensor"};
    auto base = createSimulatedDriver(record);
    auto &driver = dynamic_cast<UniversalDeviceDriver &>(*base);
    auto &device = driver.device();
    const auto &descriptor = driver.descriptor();
    require(device.getId() == record.local_id &&
                device.getName() == record.name &&
                descriptor.getLocalId() == record.local_id &&
                descriptor.getPhysicalId() == record.physical_id &&
                descriptor.getName() == record.name,
            "The protocol creator must preserve inventory identity");
    require(descriptor.getCapabilities().empty() &&
                device.getProperties().empty(),
            "The protocol creator must initialize a bare twin");

    for (const auto *id : {"on_off", "brightness", "color", "temperature",
                           "humidity", "contact", "battery", "power"}) {
        driver.addCapability(id);
        const auto *entry = CapabilityCatalog::find(id);
        require(entry && entry->read(device) == entry->initial_value &&
                    descriptor.getCapabilities().back().getId() == id,
                "The catalog must supply operations and a typed initial value");
    }
    require(device.get(props::light::Brightness) == 50 &&
                device.get(props::climate::Temperature) == 23.0 &&
                device.get(props::security::Battery) == 0,
            "Mixed capabilities must retain their own property defaults");
    driver.writeCapability("brightness", 72);
    require(device.get(props::light::Brightness) == 72,
            "A simulated write must update RAM");

    expectException<std::invalid_argument>(
        [&] { driver.addCapability("brightness"); });
    expectException<std::invalid_argument>(
        [&] { driver.addCapability("unknown"); });
    expectException<std::invalid_argument>(
        [&] { driver.writeCapability("brightness", 101); });
    expectException<std::invalid_argument>(
        [&] { driver.writeCapability("temperature", 25.0); });
    expectException<std::invalid_argument>(
        [&] { UniversalDeviceDriver invalid(nullptr, record); });
    require(descriptor.getCapabilities().size() == 8 &&
                device.get(props::light::Brightness) == 72,
            "Rejected operations must preserve values and capabilities");

    const auto &temperature = CapabilityCatalog::temperature();
    const auto &output = temperature.descriptor.getOperations()
                             .front()
                             .getOutputParameter()
                             .value();
    const auto &metadata = std::get<FloatMetadata>(output.getMetadata());
    require(metadata.min == temperature.property.min_value &&
                metadata.max == temperature.property.max_value &&
                metadata.unit == temperature.property.unit,
            "Temperature metadata must come from the property tag");

    UniversalDeviceDriver transport(record);
    int calls = 0;
    transport.addCapability(
        "on_off", [&](const std::string &id, const Value &value) {
            require(id == "on_off" && value == Value{true},
                    "Handler must receive the validated command");
            ++calls;
            throw std::runtime_error("Injected send failure");
        });
    expectException<std::invalid_argument>(
        [&] { transport.writeCapability("on_off", 1); });
    expectException<std::runtime_error>(
        [&] { transport.writeCapability("on_off", true); });
    require(calls == 1 && transport.device().get(props::light::State) == false,
            "The transport handler must run once and keep reported state on "
            "failure");
}
