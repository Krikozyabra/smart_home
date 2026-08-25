#pragma once

#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <string>
namespace smart_home {

class IDeviceDriverFactory {
  public:
    virtual ~IDeviceDriverFactory() = default;

    virtual std::string getDriverId() const = 0;
    virtual std::unique_ptr<IDeviceDriver> create(const DeviceRecord &) = 0;
};

} // namespace smart_home
