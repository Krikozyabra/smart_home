#pragma once

#include "devices/types/Color.h"

namespace smart_home {

class IColor {
  public:
    virtual ~IColor() = default;

    virtual Color getColor() const = 0;
    virtual void setColor(Color) = 0;
};

} // namespace smart_home
