/**
 * @file PropertyTags.h
 * @brief Стандартные ключи, типы и границы свойств устройств.
 */

#pragma once

#include "devices/PropertyTag.h"

namespace smart_home::props {

/// @brief Свойства включения и освещения.
namespace light {
inline constexpr PropertyTag<bool> State{"on_off.state"}; ///< Включение.
inline constexpr PropertyTag<int> Brightness{"brightness.level", 0, 100,
                                             "%"};    ///< Яркость.
inline constexpr PropertyTag<Color> Rgb{"color.rgb"}; ///< Цвет.
} // namespace light

/// @brief Свойства микроклимата.
namespace climate {
inline constexpr PropertyTag<double> Temperature{"temperature.celsius", -50.0,
                                                 100.0, "°C"}; ///< Температура.
inline constexpr PropertyTag<double> Humidity{
    "humidity.percent", 0.0, 100.0, "%"}; ///< Относительная влажность.
inline constexpr PropertyTag<double> TargetTemperature{
    "climate.target_celsius", 5.0, 35.0, "°C"};                   ///< Уставка.
inline constexpr PropertyTag<int> Co2{"co2.ppm", 0, 5000, "ppm"}; ///< CO₂.
} // namespace climate

/// @brief Свойства датчиков безопасности и их питания.
namespace security {
inline constexpr PropertyTag<bool> Motion{"motion.detected"}; ///< Движение.
inline constexpr PropertyTag<bool> Contact{"contact.open"};   ///< Контакт.
inline constexpr PropertyTag<bool> Tamper{"security.tamper"}; ///< Вскрытие.
inline constexpr PropertyTag<int> Battery{"battery.level", 0, 100,
                                          "%"}; ///< Заряд.
} // namespace security

/// @brief Электрические параметры устройства.
namespace power {
inline constexpr PropertyTag<double> Voltage{"power.voltage", 0.0, 300.0,
                                             "V"}; ///< Напряжение.
inline constexpr PropertyTag<double> Current{"power.current", 0.0, 32.0,
                                             "A"}; ///< Ток.
inline constexpr PropertyTag<double> PowerWatts{"power.active_power", 0.0,
                                                10000.0, "W"}; ///< Мощность.
} // namespace power

} // namespace smart_home::props
