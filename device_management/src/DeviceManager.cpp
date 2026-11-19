#include "device_management/DeviceManager.h"
#include "drivers/CapabilityCatalog.h"
#include "drivers/UniversalDeviceDriver.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include "logging/Logging.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace smart_home {
namespace {

std::unique_ptr<IDeviceDriver> createDriver(const DeviceCreator &creator,
                                          const DeviceRecord &record) {
    auto driver = creator(record);
    if (!driver)
        throw std::logic_error("Device creator returned nullptr");
    return driver;
}

void addDiscoveredCapabilities(IDeviceDriver &driver,
                               const std::vector<std::string> &capability_ids) {
    auto *universal = dynamic_cast<UniversalDeviceDriver *>(&driver);
    for (const auto &id : capability_ids) {
        if (driver.descriptor().findCapability(id))
            continue;
        if (!universal)
            throw std::logic_error("Driver cannot add discovered capability: " + id);
        universal->addCapability(id);
    }
}

} // namespace

DeviceManager::DeviceManager(IDeviceStorage &c_storage,
                             const DriverFactoryRegistry &c_factory_registry,
                             DeviceRegistry &c_device_registry,
                             ontology::IOntologyStorage &c_ontology_storage)
    : storage(c_storage), factory_registry(c_factory_registry),
      device_registry(c_device_registry), ontology_storage(c_ontology_storage) {}

DeviceRecord DeviceManager::getOrCreate(const std::string &physical_id,
                                        const std::string &driver_id,
                                        const std::string &default_name) {
    auto record = storage.findByPhysicalId(physical_id, driver_id);
    if (record.has_value())
        return record.value();
    return storage.insert(physical_id, driver_id, default_name);
}

void DeviceManager::syncDeviceOntology(const DeviceRecord &record,
                                       const IDeviceDriver &driver) {
    const auto node = ontology_storage.getDevice(record.local_id);
    if (!node)
        ontology_storage.addDevice({record.local_id, record.name});
    else if (node->name != record.name)
        throw std::runtime_error("Graph and inventory disagree for device " +
                                 std::to_string(record.local_id) + ".");

    std::set<std::string> linked;
    for (const auto &ability :
         ontology_storage.getDeviceAbilities(record.local_id))
        linked.insert(ability.id);

    std::set<std::string> expected;
    for (const auto &capability : driver.descriptor().getCapabilities()) {
        const auto &id = capability.getId();
        expected.insert(id);
        if (!ontology_storage.getAbility(id))
            throw std::runtime_error("Missing capability in ontology catalog: " +
                                     id + ".");
        if (linked.insert(id).second)
            ontology_storage.addDeviceAbility(record.local_id, id);
    }
    for (const auto &id : linked)
        if (expected.count(id) == 0)
            throw std::runtime_error(
                "Graph capabilities disagree with driver for device " +
                std::to_string(record.local_id) + ".");
}

std::size_t DeviceManager::restoreStoredDevices() {
    std::vector<DeviceRecord> all_records = storage.getAll();
    std::vector<std::unique_ptr<IDeviceDriver>> drivers_for_registration;
    for (const auto &record : all_records) {
        IDeviceDriver *device_driver = device_registry.find(record.local_id);
        if (device_driver != nullptr)
            continue;
        const auto *creator = factory_registry.find(record.driver_id);
        if (!creator)
            throw std::runtime_error("No device creator registered for protocol: " +
                                     record.driver_id);
        auto driver = createDriver(*creator, record);
        if (auto *universal = dynamic_cast<UniversalDeviceDriver *>(driver.get());
            universal && ontology_storage.getDevice(record.local_id)) {
            for (const auto &ability : ontology_storage.getDeviceAbilities(record.local_id))
                if (!universal->descriptor().findCapability(ability.id))
                    universal->addCapability(ability.id);
        }
        drivers_for_registration.push_back(std::move(driver));
    }
    for (auto &driver : drivers_for_registration)
        device_registry.add(std::move(driver));
    for (const auto &record : all_records)
        syncDeviceOntology(record, *device_registry.find(record.local_id));
    logging::log(logging::Level::Info, "DeviceManager", "Stored devices restored.");
    return drivers_for_registration.size();
}

DeviceRecord
DeviceManager::registerDiscoveredDevice(const std::string &physical_id,
                                        const std::string &driver_id,
                                        const std::string &default_name,
                                        const std::vector<std::string> &capability_ids) {
    const auto *creator = factory_registry.find(driver_id);
    if (!creator)
        throw UnsupportedDriverError("The driver '" + driver_id +
                                     "' is not supported");
    std::set<std::string> requested;
    for (const auto &id : capability_ids)
        if (!CapabilityCatalog::find(id) || !requested.insert(id).second)
            throw std::invalid_argument("Unknown or repeated discovered capability: " + id);
    DeviceRecord discovered_device =
        getOrCreate(physical_id, driver_id, default_name);
    IDeviceDriver *device_driver =
        device_registry.find(discovered_device.local_id);
    if (device_driver == nullptr) {
        auto driver = createDriver(*creator, discovered_device);
        addDiscoveredCapabilities(*driver, capability_ids);
        device_driver = driver.get();
        device_registry.add(std::move(driver));
    } else
        addDiscoveredCapabilities(*device_driver, capability_ids);
    syncDeviceOntology(discovered_device, *device_driver);
    logging::Context context;
    context.device_id = discovered_device.local_id;
    logging::log(logging::Level::Debug, "DeviceManager", "Discovered device registered.", context);
    return discovered_device;
}

} // namespace smart_home
