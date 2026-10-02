/**
 * @file SimulatedLightDriver.h
 * @brief Драйвер симулированного источника света для системы умного дома.
 */

#pragma once

#include "common/DeviceId.h"
#include "descriptors/DeviceDescriptor.h"
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
  private:
    /**
     * @brief Генерирует дескриптор устройства симулированного источника света.
     *
     * Описание позиционных параметров:
     * - Первый параметр (DeviceId): идентификатор устройства.
     * - Второй параметр (const std::string &): физический идентификатор устройства.
     * - Третий параметр (const std::string &): имя устройства.
     *
     * @return Сгенерированный дескриптор устройства DeviceDescriptor.
     */
    static DeviceDescriptor generateDeviceDescriptor(DeviceId,
                                                     const std::string &,
                                                     const std::string &);

  public:
    /**
     * @brief Создает экземпляр драйвера симулированного источника света.
     *
     * Описание позиционных параметров:
     * - Первый параметр (DeviceId): уникальный идентификатор устройства.
     * - Второй параметр (std::string): физический идентификатор устройства.
     * - Третий параметр (std::string): имя устройства.
     * - Четвертый параметр (bool): начальное состояние питания (включено/выключено).
     * - Пятый параметр (uint8_t): начальный уровень яркости.
     * - Шестой параметр (Color): начальный цвет освещения.
     */
    SimulatedLightDriver(DeviceId, std::string, std::string, bool, uint8_t,
                         Color);
};

} // namespace smart_home
