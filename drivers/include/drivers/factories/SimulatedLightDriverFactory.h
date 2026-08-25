#pragma once

#include "IDeviceDriverFactory.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <string>

namespace smart_home {

class SimulatedLightDriverFactory final : public IDeviceDriverFactory {
  public:
    std::string getDriverId() const override;
    std::unique_ptr<IDeviceDriver> create(const DeviceRecord &) override;
};

} // namespace smart_home
