#include "cli/DeviceOutput.h"
#include "descriptors/DeviceDescriptor.h"

#include <sstream>
#include <type_traits>

namespace smart_home::cli {
namespace {

void writeValue(std::ostream &out, const Value &value) {
    std::visit(
        [&](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, Color>)
                out << static_cast<int>(item.red) << ' '
                    << static_cast<int>(item.green) << ' '
                    << static_cast<int>(item.blue);
            else if constexpr (std::is_same_v<T, bool>)
                out << (item ? "true" : "false");
            else
                out << item;
        },
        value);
}

} // namespace

std::string_view deviceCategory(const DeviceDescriptor &descriptor) {
    bool measures_temperature = false;
    for (const auto &capability : descriptor.getCapabilities()) {
        const auto &id = capability.getId();
        if (id == "on_off" || id == "brightness")
            return "light";
        measures_temperature |= id == "temperature";
    }
    return measures_temperature ? "temperature_sensor" : "device";
}

void writeDeviceSummary(ICliOutputSink &sink, DeviceId id,
                        std::string_view name, std::string_view category,
                        std::string_view room) {
    std::ostringstream line;
    line << "Device " << id << ' ' << name << ' ' << category
         << " room=" << room;
    sink.writeLine(line.str());
}

void writeDeviceProperty(ICliOutputSink &sink, std::string_view property,
                         const Value &value) {
    std::ostringstream line;
    line << "  " << property << " = ";
    writeValue(line, value);
    sink.writeLine(line.str());
}

void writeDeviceResult(ICliOutputSink &sink, DeviceId id,
                       std::string_view operation,
                       const std::optional<Value> &result) {
    std::ostringstream line;
    line << "Device " << id << ' ' << operation << ": ";
    if (result)
        writeValue(line, *result);
    else
        line << "applied";
    sink.writeLine(line.str());
}

void writeDeviceError(ICliOutputSink &sink, DeviceId id,
                      std::string_view reason) {
    std::ostringstream line;
    line << "Device " << id << " error: " << reason;
    sink.writeError(line.str());
}

} // namespace smart_home::cli
