#pragma once

#include "types/Color.h"

class IColor {
    public:
    virtual ~IColor() = default;

    virtual Color getColor() const = 0;
    virtual void setColor(Color) = 0;
};
