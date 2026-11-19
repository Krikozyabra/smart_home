/**
 * @file devices/types/Value.h
 * @brief Тип значения свойств устройств и аргументов команд.
 */

#pragma once

#include "devices/types/Color.h"

#include <string>
#include <variant>

namespace smart_home {

/// @brief Хранимое по значению свойство, аргумент или результат операции.
using Value = std::variant<bool, int, double, char, Color, std::string>;

} // namespace smart_home
