/**
 * @file ITemperature.h
 * @brief Интерфейс устройства для измерения температуры.
 */

#pragma once

namespace smart_home {

/**
 * @brief Интерфейс для устройств, предоставляющих данные о температуре.
 */
class ITemperature {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~ITemperature() = default;

    /**
     * @brief Получить текущее значение температуры.
     * @return Текущая температура в градусах.
     */
    virtual double getTemperature() const = 0;
};

} // namespace smart_home
