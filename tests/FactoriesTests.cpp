#include "TestUtil.h"
#include "drivers/factories/SimulatedLightDriverFactory.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"
#include <memory>
#include <stdexcept>

int main() {
  smart_home::SimulatedLightDriverFactory sl_factory;
  require(sl_factory.getDriverId() == "simulated.light",
          "Factory must return correct driver id");
  smart_home::DeviceRecord data_for_factory{1, "AC:12:DD", "simulated.light",
                                            "Light number 1"};
  std::unique_ptr<smart_home::IDeviceDriver> generated_driver =
      sl_factory.create(data_for_factory);
  require(generated_driver != nullptr,
          "Driver factory must not return nullptr");
  auto &device = generated_driver->device();
  require(device.getId() == data_for_factory.local_id,
          "Generated device must have id like in record");
  require(device.getName() == data_for_factory.name,
          "Generated device must have name like in record");
  auto &descriptor = generated_driver->descriptor();
  require(descriptor.getLocalId() == data_for_factory.local_id,
          "Generated descriptor must have id like in record");
  require(descriptor.getName() == data_for_factory.name,
          "Generated descriptor must have name like in record");
  require(descriptor.getPhysicalId() == data_for_factory.physical_id,
          "Generated descriptor must have id like in record");

  smart_home::DeviceRecord broken_data_for_factory{
      2, "BB:00:01", "zigbee.light", "BROKEN LIGHT"};
  expectException<std::invalid_argument>(
      [&] { sl_factory.create(broken_data_for_factory); });
}
