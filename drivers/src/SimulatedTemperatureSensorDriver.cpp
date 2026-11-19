#include "drivers/SimulatedTemperatureSensorDriver.h"

#include "descriptors/DeviceDescriptor.h"
#include "devices/Device.h"
#include "drivers/CapabilityCatalog.h"

#include <memory>
#include <utility>

namespace smart_home {

SimulatedTemperatureSensorDriver::SimulatedTemperatureSensorDriver(
    DeviceId local_id, std::string physical_id, std::string name,
    double temperature)
    : DeviceDriverBase(
          std::make_unique<Device>(local_id, name),
          DeviceDescriptor(local_id, std::move(physical_id), name,
                           {CapabilityCatalog::temperature().descriptor})) {
    device().set(CapabilityCatalog::temperature().property, temperature);
}

} // namespace smart_home
