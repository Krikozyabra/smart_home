#pragma once

/**
 * @file SimulatedTemperatureSensorDriver.h
 * @brief Драйвер симулированного датчика температуры.
 */

#include "drivers/DeviceDriverBase.h"
#include "common/DeviceId.h"
#include "descriptors/DeviceDescriptor.h"

#include <string>

namespace smart_home {

/**
 * @class SimulatedTemperatureSensorDriver
 * @brief Класс драйвера симулированного датчика температуры.
 */
class SimulatedTemperatureSensorDriver final : public DeviceDriverBase {
  private:
    /**
     * @brief Генерирует дескриптор устройства для симулированного датчика температуры.
     * Первый параметр — идентификатор устройства (DeviceId).
     * Второй параметр — физический идентификатор устройства (const std::string &).
     * Третий параметр — имя устройства (const std::string &).
     * @return Сформированный дескриптор устройства DeviceDescriptor.
     */
    static DeviceDescriptor generateDeviceDescriptor(DeviceId,
                                                     const std::string &,
                                                     const std::string &);

  public:
    /**
     * @brief Конструктор драйвера симулированного датчика температуры.
     * Первый параметр — идентификатор устройства (DeviceId).
     * Второй параметр — физический идентификатор устройства (std::string).
     * Третий параметр — имя устройства (std::string).
     * Четвертый параметр — начальное значение температуры (double).
     */
    SimulatedTemperatureSensorDriver(DeviceId, std::string, std::string,
                                     double);
};

} // namespace smart_home
