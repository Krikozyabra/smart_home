#pragma once

#include "devices/types/Color.h"

#include <string>
#include <variant>

namespace smart_home {

using Value = std::variant<bool, int, double, char, Color, std::string>;

}
