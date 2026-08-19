#pragma once

#include <cstdint>

namespace smart_home {

class IBrightness {
  public:
    virtual ~IBrightness() = default;

    virtual std::uint8_t getBrightness() const = 0;
    virtual void setBrightness(std::uint8_t) = 0;
};

} // namespace smart_home
