#include "registry/DeviceRegistry.h"
#include "drivers/interfaces/IDeviceDriver.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace smart_home {

void DeviceRegistry::add(std::unique_ptr<IDeviceDriver> driver) {
    if (driver == nullptr)
        throw std::invalid_argument("Driver must not be nullptr");

    unsigned int local_id = driver->device().getId();

    auto [iterator, inserted] =
        drivers.try_emplace(local_id, std::move(driver));

    if (!inserted)
        throw std::runtime_error(
            "Drivers local_id=" + std::to_string(local_id) +
            " already registred");
}

const IDeviceDriver *DeviceRegistry::find(unsigned int local_id) const {
    auto iterator = drivers.find(local_id);
    if (iterator == drivers.end())
        return nullptr;

    return iterator->second.get();
}

IDeviceDriver *DeviceRegistry::find(unsigned int local_id) {
    auto iterator = drivers.find(local_id);
    if (iterator == drivers.end())
        return nullptr;

    return iterator->second.get();
}

std::size_t DeviceRegistry::size() const { return drivers.size(); }

} // namespace smart_home
