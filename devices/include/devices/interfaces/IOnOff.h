#pragma once

namespace smart_home {

class IOnOff {
  public:
    virtual ~IOnOff() = default;

    virtual void setOn(bool) = 0;
    virtual bool isOn() const = 0;
};

} // namespace smart_home
