#pragma once

#include <Device.h>
#include <DeviceDescriptor.h>

namespace smart_home{

class IDeviceDriver{
public:
    virtual ~IDeviceDriver() = default;
    
    virtual Device& device() = 0;
    virtual const DeviceDescriptor& descriptor() const = 0;
};

}
