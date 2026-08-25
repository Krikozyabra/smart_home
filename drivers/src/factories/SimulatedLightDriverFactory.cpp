#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace smart_home {

std::string SimulatedLightDriverFactory::getDriverId() const {
    return "simulated.light";
}

std::unique_ptr<IDeviceDriver>
SimulatedLightDriverFactory::create(const DeviceRecord &record) {
    if (record.driver_id != getDriverId())
        throw std::invalid_argument(
            "SimulatedLightDriverFactory cannot create driver for '" +
            record.driver_id + "'");
    return std::make_unique<SimulatedLightDriver>(
        record.local_id, record.physical_id, record.name, false, 50,
        Color{255, 255, 255});
}

} // namespace smart_home
