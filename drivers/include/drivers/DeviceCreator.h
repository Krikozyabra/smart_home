#pragma once

#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"

#include <functional>
#include <memory>

namespace smart_home {

/**
 * @brief Создатель драйвера, зарегистрированный по идентификатору протокола.
 * @details Получает запись инвентаря на время вызова и передаёт владение
 * драйвером.
 */
using DeviceCreator =
    std::function<std::unique_ptr<IDeviceDriver>(const DeviceRecord &)>;

/**
 * @brief Создаёт базовый симулированный драйвер с записью в память.
 * @throws std::invalid_argument Если запись относится к другому протоколу.
 */
std::unique_ptr<IDeviceDriver>
createSimulatedDriver(const DeviceRecord &record);

} // namespace smart_home
