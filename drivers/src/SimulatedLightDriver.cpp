#include "drivers/SimulatedLightDriver.h"

#include "descriptors/DeviceDescriptor.h"
#include "devices/Device.h"
#include "drivers/CapabilityCatalog.h"

#include <memory>
#include <utility>

namespace smart_home {

SimulatedLightDriver::SimulatedLightDriver(DeviceId local_id,
                                           std::string physical_id,
                                           std::string name, bool is_on,
                                           std::uint8_t brightness, Color color)
    : DeviceDriverBase(
          std::make_unique<Device>(local_id, name),
          DeviceDescriptor(local_id, std::move(physical_id), name,
                           {CapabilityCatalog::onOff().descriptor,
                            CapabilityCatalog::brightness().descriptor,
                            CapabilityCatalog::color().descriptor})) {
    device().set(CapabilityCatalog::onOff().property, is_on);
    device().set(CapabilityCatalog::brightness().property,
                 static_cast<int>(brightness));
    device().set(CapabilityCatalog::color().property, color);
}

} // namespace smart_home
