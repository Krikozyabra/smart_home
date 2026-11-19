/**
 * @file PropertyTag.h
 * @brief Типизированные метки свойств устройства.
 */

#pragma once

#include "devices/types/Value.h"

#include <type_traits>

namespace smart_home {

/**
 * @brief Связывает ключ числового или логического свойства с типом Value.
 * @tparam T Один из типов bool, int, double, char.
 * @details Обе границы включаются в интервал и применяются независимо.
 * Ключ и единица указывают на строки с завершающим нулём, которые должны
 * оставаться доступными при использовании метки.
 */
template <typename T> struct PropertyTag {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                      std::is_same_v<T, double> || std::is_same_v<T, char>,
                  "PropertyTag requires a supported Value type");

    using Type = T; ///< Тип значения свойства.

    const char *key;      ///< Ключ свойства.
    const char *unit;     ///< Единица измерения или пустая строка.
    bool has_min = false; ///< Признак наличия нижней границы.
    T min_value{};        ///< Нижняя граница при has_min.
    bool has_max = false; ///< Признак наличия верхней границы.
    T max_value{};        ///< Верхняя граница при has_max.

    /// @brief Создаёт метку без ограничения диапазона.
    constexpr explicit PropertyTag(const char *key,
                                   const char *unit = "") noexcept
        : key(key), unit(unit) {}

    /// @brief Создаёт метку с включительными границами.
    constexpr PropertyTag(const char *key, T min_value, T max_value,
                          const char *unit = "") noexcept
        : key(key), unit(unit), has_min(true), min_value(min_value),
          has_max(true), max_value(max_value) {}

    /// @brief Проверяет действующие границы. NaN вне любого заданного
    /// диапазона.
    constexpr bool validate(const T &value) const noexcept {
        return (!has_min || min_value <= value) &&
               (!has_max || value <= max_value);
    }
};

/**
 * @brief Метка строкового свойства без границ.
 * @details Ключ и единица должны указывать на действительные строки
 * с завершающим нулём при каждом использовании метки.
 */
template <> struct PropertyTag<std::string> {
    using Type = std::string; ///< Тип значения свойства.

    const char *key;  ///< Ключ свойства.
    const char *unit; ///< Единица измерения или пустая строка.

    /// @brief Создаёт метку строкового свойства.
    constexpr explicit PropertyTag(const char *key,
                                   const char *unit = "") noexcept
        : key(key), unit(unit) {}

    /// @brief Принимает любое строковое значение.
    constexpr bool validate(const std::string &) const noexcept { return true; }
};

/**
 * @brief Метка RGB-свойства без границ.
 * @details Ключ и единица должны указывать на действительные строки
 * с завершающим нулём при каждом использовании метки.
 */
template <> struct PropertyTag<Color> {
    using Type = Color; ///< Тип значения свойства.

    const char *key;  ///< Ключ свойства.
    const char *unit; ///< Единица измерения или пустая строка.

    /// @brief Создаёт метку RGB-свойства.
    constexpr explicit PropertyTag(const char *key,
                                   const char *unit = "") noexcept
        : key(key), unit(unit) {}

    /// @brief Принимает любую комбинацию RGB-компонентов.
    constexpr bool validate(const Color &) const noexcept { return true; }
};

} // namespace smart_home
