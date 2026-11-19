#include "descriptors/OperationDescriptor.h"
#include "descriptors/ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "devices/views/TemperatureView.h"
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

    return Value{device.view<TemperatureView>().getTemperature()};
}

} // namespace smart_home
