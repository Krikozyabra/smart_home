/**
 * @file IOnOff.h
 * @brief Интерфейс устройства с поддержкой включения и выключения.
 */

#pragma once

namespace smart_home {

/**
 * @brief Интерфейс для устройств, поддерживающих включение и выключение.
 */
class IOnOff {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IOnOff() = default;

    /**
     * @brief Устанавливает состояние включения устройства.
     *
     * Первый параметр задает новое состояние устройства (true — включено, false — выключено).
     */
    virtual void setOn(bool) = 0;

    /**
     * @brief Проверяет, включено ли устройство.
     * @return true, если устройство включено, иначе false.
     */
    virtual bool isOn() const = 0;
};

} // namespace smart_home
