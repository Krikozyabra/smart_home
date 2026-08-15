#pragma once

class IOnOff {
  public:
    virtual ~IOnOff() = default;

    virtual void setOn(bool) = 0;
    virtual bool isOn() const = 0;
};
