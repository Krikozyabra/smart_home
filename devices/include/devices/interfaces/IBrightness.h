/**
 * @file IBrightness.h
 * @brief Интерфейс устройства с поддержкой управления яркостью.
 */

#pragma once

#include <cstdint>

namespace smart_home {

/**
 * @brief Интерфейс для устройств, поддерживающих регулировку уровня яркости.
 */
class IBrightness {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IBrightness() = default;

    /**
     * @brief Возвращает текущий уровень яркости устройства.
     * @return Текущий уровень яркости.
     */
    virtual std::uint8_t getBrightness() const = 0;

    /**
     * @brief Устанавливает уровень яркости устройства.
     *
     * Первый параметр задает новый уровень яркости устройства (std::uint8_t).
     */
    virtual void setBrightness(std::uint8_t) = 0;
};

} // namespace smart_home
