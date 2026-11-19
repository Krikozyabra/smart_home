/**
 * @file SimulatedLightDriver.h
 * @brief Драйвер симулированного источника света для системы умного дома.
 */

#pragma once

#include "common/DeviceId.h"
#include "devices/types/Color.h"
#include "drivers/DeviceDriverBase.h"

#include <cstdint>
#include <string>

namespace smart_home {

/**
 * @class SimulatedLightDriver
 * @brief Драйвер для симуляции управляемого источника света (умной лампы).
 */
class SimulatedLightDriver final : public DeviceDriverBase {
  public:
    /**
     * @brief Создает экземпляр драйвера симулированного источника света.
     * @param local_id Локальный идентификатор.
     * @param physical_id Физический идентификатор.
     * @param name Имя устройства.
     * @param is_on Начальное состояние включения.
     * @param brightness Начальная яркость в процентах.
     * @param color Начальный цвет RGB.
     * @throws std::invalid_argument Если яркость выше 100.
     */
    SimulatedLightDriver(DeviceId local_id, std::string physical_id,
                         std::string name, bool is_on, std::uint8_t brightness,
                         Color color);
};

} // namespace smart_home
