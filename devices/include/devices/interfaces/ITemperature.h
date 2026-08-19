#pragma once

namespace smart_home {

class ITemperature {
  public:
    virtual ~ITemperature() = default;
    virtual double getTemperature() const = 0;
};

} // namespace smart_home
