#pragma once

/**
 * @file SimulatedTemperatureSensorDriver.h
 * @brief Драйвер симулированного датчика температуры.
 */

#include "common/DeviceId.h"
#include "drivers/DeviceDriverBase.h"

#include <string>

namespace smart_home {

/**
 * @class SimulatedTemperatureSensorDriver
 * @brief Класс драйвера симулированного датчика температуры.
 */
class SimulatedTemperatureSensorDriver final : public DeviceDriverBase {
  public:
    /**
     * @brief Конструктор драйвера симулированного датчика температуры.
     * @param local_id Локальный идентификатор.
     * @param physical_id Физический идентификатор.
     * @param name Имя устройства.
     * @param temperature Начальная температура в градусах Цельсия.
     * @throws std::invalid_argument Если температура вне диапазона [-50, 100].
     */
    SimulatedTemperatureSensorDriver(DeviceId local_id, std::string physical_id,
                                     std::string name, double temperature);
};

} // namespace smart_home
