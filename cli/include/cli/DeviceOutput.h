#pragma once

#include "cli/ICliOutputSink.h"
#include "common/DeviceId.h"
#include "devices/types/Value.h"

#include <optional>

namespace smart_home {
class DeviceDescriptor;
}

namespace smart_home::cli {

/** @brief Определяет категорию CLI по возможностям дескриптора.
 * @return Статическая строка `light` при наличии `on_off` или `brightness`,
 * иначе `temperature_sensor` при наличии `temperature`, иначе `device`.
 */
std::string_view deviceCategory(const DeviceDescriptor &descriptor);

/// @brief Выводит локальный ID, имя, категорию и комнату устройства.
void writeDeviceSummary(ICliOutputSink &sink, DeviceId id,
                        std::string_view name, std::string_view category,
                        std::string_view room);

/// @brief Выводит строку с именем и текущим значением свойства для команды
/// show.
void writeDeviceProperty(ICliOutputSink &sink, std::string_view property,
                         const Value &value);

/// @brief Выводит результат чтения или `applied` для записи без значения.
void writeDeviceResult(ICliOutputSink &sink, DeviceId id,
                       std::string_view operation,
                       const std::optional<Value> &result);

/// @brief Направляет ошибку выполнения для указанного ID в канал ошибок.
void writeDeviceError(ICliOutputSink &sink, DeviceId id,
                      std::string_view reason);

} // namespace smart_home::cli
