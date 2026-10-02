/**
 * @file Value.h
 * @brief Определение типа значения Value для команд и свойств устройств.
 */

#pragma once

#include "devices/types/Color.h"

#include <string>
#include <variant>

namespace smart_home {

/**
 * @brief Универсальный тип данных для представления значений аргументов и свойств.
 */
using Value = std::variant<bool, int, double, char, Color, std::string>;

}
