#pragma once

#include <cstdint>
namespace smart_home{

enum class OperationType : std::uint8_t { Read, Write, Event };

enum class ValueType : std::uint8_t { Boolean, Integer, Float, Color, String };

}
