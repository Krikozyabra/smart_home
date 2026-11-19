#include "TestUtil.h"
#include "storage/DeviceRecord.h"
#include "storage/InMemoryDeviceStorage.h"

#include <optional>
#include <stdexcept>

int main() {
  smart_home::InMemoryDeviceStorage storage;
  require(storage.getAll().empty(), "Initial storage must be empty");
  require(storage.findByPhysicalId("Physical id", "Driver id") == std::nullopt,
          "Unstored physical id must return nullopt");

  require(storage.insert("AC:12:DD", "wifi", "Light") ==
              smart_home::DeviceRecord{0, "AC:12:DD", "wifi", "Light"},
          "storage.insert must return correct inserted object");
  require(storage.findByPhysicalId("AC:12:DD", "wifi") ==
              smart_home::DeviceRecord{0, "AC:12:DD", "wifi", "Light"},
          "finding by physical id must return correct record");
  require(storage.insert("AC:12:DD", "zigbee", "TemperatureSensor") ==
              smart_home::DeviceRecord{1, "AC:12:DD", "zigbee",
                                       "TemperatureSensor"},
          "storage.insert must insert next object with another local_id");
  require(storage.findByPhysicalId("AC:12:DD", "bluetooth") == std::nullopt,
          "finding by physical id must take driver id into account");
  expectException<std::invalid_argument>(
      [&] { storage.insert("AC:12:DD", "zigbee", "Duplicate TS"); });
  expectException<std::invalid_argument>(
      [&] { storage.insert("", "wifi", "Empty physical_id"); });
  expectException<std::invalid_argument>(
      [&] { storage.insert("D0:CC:00", "", "Empty driver_id"); });
  expectException<std::invalid_argument>(
      [&] { storage.findByPhysicalId("", "wifi"); });
  expectException<std::invalid_argument>(
      [&] { storage.findByPhysicalId("D0:CC:00", ""); });

  require(storage.getAll().size() == 2, "All records must be in memory");

  smart_home::IDeviceStorage &inventory = storage;
  inventory.close();
  inventory.close();
  expectException<std::logic_error>([&] { inventory.getAll(); });
  expectException<std::logic_error>(
      [&] { inventory.findByPhysicalId("AC:12:DD", "wifi"); });
  expectException<std::logic_error>(
      [&] { inventory.insert("new", "wifi", "Name"); });
  expectException<std::logic_error>([&] { inventory.dumpToFile("unused.db"); });
  try {
    inventory.insert("", "", "");
    require(false, "Closed storage must reject invalid insert arguments");
  } catch (const std::logic_error &error) {
    require(std::string(error.what()) == "Storage is closed.",
            "Closed state is checked before insert arguments");
  }
  try {
    inventory.findByPhysicalId("", "");
    require(false, "Closed storage must reject invalid lookup arguments");
  } catch (const std::logic_error &error) {
    require(std::string(error.what()) == "Storage is closed.",
            "Closed state is checked before lookup arguments");
  }

  smart_home::InMemoryDeviceStorage empty;
  empty.close();
  empty.close();
  expectException<std::logic_error>([&] { empty.getAll(); });
}
