#include "registry/DriverFactoryRegistry.h"
#include "drivers/factories/IDeviceDriverFactory.h"
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace smart_home {

void DriverFactoryRegistry::add(std::unique_ptr<IDeviceDriverFactory> factory) {
  if (factory == nullptr)
    throw std::invalid_argument(
        "DriverFactoryRegistry cannot accept nullptr factory");
  if (factory->getDriverId().empty())
    throw std::invalid_argument("Factory must not have empty linked driver id");
  auto [iterator, inserted] =
      factories.try_emplace(factory->getDriverId(), std::move(factory));
  if (!inserted)
    throw std::runtime_error("Factory for driver id = '" +
                             factory->getDriverId() + "' already in registry");
}

IDeviceDriverFactory *
DriverFactoryRegistry::find(const std::string &driver_id) {
  auto iterator = factories.find(driver_id);
  if (iterator == factories.end())
    return nullptr;
  return iterator->second.get();
}

const IDeviceDriverFactory *
DriverFactoryRegistry::find(const std::string &driver_id) const {
  auto iterator = factories.find(driver_id);
  if (iterator == factories.end())
    return nullptr;
  return iterator->second.get();
}
std::size_t DriverFactoryRegistry::size() const noexcept {
  return factories.size();
}

} // namespace smart_home
