/**
 * @file Color.h
 * @brief Определение структуры цвета в формате RGB.
 */

#pragma once

#include <cstdint>

/**
 * @brief Структура, представляющая цвет в формате RGB.
 */
struct Color {
    /**
     * @brief Красная составляющая цвета (0–255).
     */
    std::uint8_t red;

    /**
     * @brief Зеленая составляющая цвета (0–255).
     */
    std::uint8_t green;

    /**
     * @brief Синяя составляющая цвета (0–255).
     */
    std::uint8_t blue;

    /**
     * @brief Оператор сравнения на равенство с константной ссылкой.
     * @param other Другой объект Color для сравнения.
     * @return true, если все компоненты цвета совпадают, иначе false.
     */
    bool operator==(const Color &other) const noexcept {
        return ((red == other.red) && (green == other.green) &&
                (blue == other.blue));
    }

    /**
     * @brief Оператор сравнения на равенство с перемещаемым объектом.
     * @param other Перемещаемый объект Color для сравнения.
     * @return true, если все компоненты цвета совпадают, иначе false.
     */
    bool operator==(Color &&other) const noexcept {
        return ((red == other.red) && (green == other.green) &&
                (blue == other.blue));
    }
};
