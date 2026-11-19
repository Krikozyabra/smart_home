#include "TestUtil.h"
#include "drivers/DeviceCreator.h"
#include "registry/DeviceRegistry.h"

#include <stdexcept>

int main() {
  smart_home::DeviceRegistry registry;
  const auto &const_registry = registry;
  require(registry.size() == 0, "Initial registry must have 0 devices");
  require(registry.find(1000) == nullptr,
          "Unstored device id in registry must return nullptr");
  require(const_registry.find(1000) == nullptr,
          "Unstored device in const registry id must return nullptr");

  const smart_home::DeviceRecord record{0, "SIM-LIGHT-001", "simulated", "LIGHT"};
  registry.add(smart_home::createSimulatedDriver(record));

  require(registry.size() == 1, "Registry must insert corret device");
  require(registry.find(0) != nullptr, "Registry must find inserted driver");
  require(const_registry.find(0) != nullptr,
          "Const registry must find inserted driver");

  expectException<std::runtime_error>([&] {
    registry.add(smart_home::createSimulatedDriver(record));
  });

  expectException<std::invalid_argument>([&] { registry.add(nullptr); });
}
