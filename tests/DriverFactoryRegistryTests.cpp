#include "TestUtil.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "registry/DriverFactoryRegistry.h"
#include <memory>
#include <stdexcept>

int main() {
  smart_home::DriverFactoryRegistry registry;
  const auto &const_registry = registry;
  require(registry.size() == 0, "Initial registry must be empty");
  require(registry.find("unknown.driver") == nullptr,
          "Try to find unregistered driver must return nullptr");
  require(const_registry.find("unknown.driver") == nullptr,
          "Try to const fing unregistered driver must return nullptr");

  expectException<std::invalid_argument>([&] { registry.add(nullptr); });

  registry.add(std::make_unique<smart_home::SimulatedLightDriverFactory>());
  require(registry.size() == 1, "Adding correct factory must increase size");
  require(registry.find("simulated.light") != nullptr,
          "Try to find correct driver id must mot return nullptr");
  require(const_registry.find("simulated.light") != nullptr,
          "Try to const find correct driver id must not return nullptr");

  expectException<std::runtime_error>([&] {
    registry.add(std::make_unique<smart_home::SimulatedLightDriverFactory>());
  });
}
