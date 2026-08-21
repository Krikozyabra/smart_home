#include "OperationDescriptor.h"
#include "ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "devices/interfaces/ITemperature.h"
#include "execution/CommandDispatcher.h"

#include <optional>
#include <stdexcept>
#include <variant>

namespace smart_home {

std::optional<Value>
CommandDispatcher::executeTemperatureGet(Device &device,
                                         const OperationDescriptor &operation,
                                         const Command &command) {
    const auto &operation_output_parameter =
        validateNoInputRead(operation, command);

    const auto *operation_metadata =
        std::get_if<FloatMetadata>(&operation_output_parameter.getMetadata());
    if (operation_metadata == nullptr)
        throw std::logic_error(
            "temperature.get descriptor must use FloatMetadata");

    const auto *temperature_interface =
        dynamic_cast<const ITemperature *>(&device);
    if (temperature_interface == nullptr)
        throw std::logic_error(
            "Descriptor declares temperature.get, but device "
            "does not implement ITemperature");
    return Value{temperature_interface->getTemperature()};
}

} // namespace smart_home
