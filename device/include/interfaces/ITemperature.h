#pragma once

class ITemperature{
public:
    virtual ~ITemperature() = default;
    virtual double getTemperature() const = 0;
};
