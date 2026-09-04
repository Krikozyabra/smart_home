#include "TestUtil.h"
#include "devices/types/Color.h"
#include "drivers/SimulatedLightDriver.h"
#include "registry/DeviceRegistry.h"

#include <memory>
#include <stdexcept>

int main() {
  smart_home::DeviceRegistry registry;
  const auto &const_registry = registry;
  require(registry.size() == 0, "Initial registry must have 0 devices");
  require(registry.find(1000) == nullptr,
          "Unstored device id in registry must return nullptr");
  require(const_registry.find(1000) == nullptr,
          "Unstored device in const registry id must return nullptr");

  registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
      0, "SIM-LIGHT-001", "LIGHT", false, 50, Color{255, 255, 255}));

  require(registry.size() == 1, "Registry must insert corret device");
  require(registry.find(0) != nullptr, "Registry must find inserted driver");
  require(const_registry.find(0) != nullptr,
          "Const registry must find inserted driver");

  expectException<std::runtime_error>([&] {
    registry.add(std::make_unique<smart_home::SimulatedLightDriver>(
        0, "SIM-LIGHT-001", "LIGHT", false, 50, Color{255, 255, 255}));
  });

  expectException<std::invalid_argument>([&] { registry.add(nullptr); });
}
