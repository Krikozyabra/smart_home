#pragma once

#include "devices/Device.h"

#include <descriptors/DeviceDescriptor.h>

namespace smart_home{

class IDeviceDriver{
public:
    virtual ~IDeviceDriver() = default;
    
    virtual Device& device() = 0;
    virtual const DeviceDescriptor& descriptor() const = 0;
};

}
