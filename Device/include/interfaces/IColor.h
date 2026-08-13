#pragma once

#include "utility/color_type.h"

class IColor {
    public:
    virtual ~IColor() = default;

    virtual Color getColor() const = 0;
    virtual void setColor(Color) = 0;
};
