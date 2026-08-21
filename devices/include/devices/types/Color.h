#pragma once

#include <cstdint>

struct Color {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

    bool operator==(const Color &other) const noexcept {
        return ((red == other.red) && (green == other.green) &&
                (blue == other.blue));
    }

    bool operator==(Color &&other) const noexcept {
        return ((red == other.red) && (green == other.green) &&
                (blue == other.blue));
    }
};
