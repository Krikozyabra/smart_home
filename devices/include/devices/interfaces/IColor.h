/**
 * @file IColor.h
 * @brief Интерфейс устройства с поддержкой управления цветом.
 */

#pragma once

#include "devices/types/Color.h"

namespace smart_home {

/**
 * @brief Интерфейс для устройств, поддерживающих изменение цвета.
 */
class IColor {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IColor() = default;

    /**
     * @brief Возвращает текущий цвет устройства.
     * @return Текущий цвет устройства.
     */
    virtual Color getColor() const = 0;

    /**
     * @brief Устанавливает цвет устройства.
     *
     * Первый параметр задает новый цвет устройства (Color).
     */
    virtual void setColor(Color) = 0;
};

} // namespace smart_home
