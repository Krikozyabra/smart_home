#pragma once

#include "common/DeviceId.h"
#include <string>

namespace smart_home {

class Device {
  public:
    Device(DeviceId, std::string);
    virtual ~Device() = default;

    DeviceId getId() const;

    std::string getName() const; 
    void setName(std::string);

  private:
    std::string name;
    DeviceId id;
};

} // namespace smart_home
