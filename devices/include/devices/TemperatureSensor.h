/**
 * @file TemperatureSensor.h
 * @brief Заголовочный файл класса датчика температуры TemperatureSensor.
 */

#pragma once

#include "Device.h"
#include "common/DeviceId.h"
#include "interfaces/ITemperature.h"

namespace smart_home {

/**
 * @brief Класс датчика температуры.
 *
 * Предоставляет функциональность для измерения и получения текущей температуры.
 */
class TemperatureSensor : public Device, public ITemperature {
  public:
    /**
     * @brief Конструктор датчика температуры.
     *
     * Первый параметр задает идентификатор устройства (DeviceId).
     * Второй параметр задает название устройства (std::string).
     * Третий параметр задает начальное значение температуры (double).
     */
    TemperatureSensor(DeviceId, std::string, double);

    /**
     * @brief Возвращает текущее значение температуры.
     * @return Текущая измеренная температура.
     */
    double getTemperature() const override;

  private:
    /**
     * @brief Текущее значение температуры, измеренное датчиком.
     */
    double temperature;
};

} // namespace smart_home
