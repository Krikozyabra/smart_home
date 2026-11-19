#include "drivers/UniversalDeviceDriver.h"

#include "drivers/CapabilityCatalog.h"
#include "drivers/DeviceCreator.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace smart_home {
namespace {

const CapabilityCatalog::Definition &
requireWritable(const IDeviceDriver &driver, const std::string &id) {
    const auto *entry = CapabilityCatalog::find(id);
    const auto *capability = driver.descriptor().findCapability(id);
    if (!entry || !entry->writable() || !capability)
        throw std::invalid_argument("Capability does not support writing: " +
                                    id);
    const auto &operations = capability->getOperations();
    const std::string write_id = id + ".set";
    if (std::none_of(operations.begin(), operations.end(), [&](const auto &op) {
            return op.getType() == OperationType::Write &&
                   op.getId() == write_id;
        }))
        throw std::invalid_argument("Capability does not support writing: " +
                                    id);
    return *entry;
}

} // namespace

void IDeviceDriver::writeCapability(const std::string &capability_id,
                                    const Value &value) {
    requireWritable(*this, capability_id).store(device(), value);
}

UniversalDeviceDriver::UniversalDeviceDriver(const DeviceRecord &record,
                                             ActionHandler transport)
    : UniversalDeviceDriver(
          std::make_unique<Device>(record.local_id, record.name), record,
          std::move(transport)) {}

UniversalDeviceDriver::UniversalDeviceDriver(std::unique_ptr<Device> device,
                                             const DeviceRecord &record,
                                             ActionHandler transport)
    : DeviceDriverBase(std::move(device),
                       {record.local_id, record.physical_id, record.name, {}}),
      transport_(std::move(transport)) {}

void UniversalDeviceDriver::addCapability(const std::string &capability_id,
                                          ActionHandler handler) {
    const auto *entry = CapabilityCatalog::find(capability_id);
    if (!entry)
        throw std::invalid_argument("Unknown capability: " + capability_id);
    if (descriptor().findCapability(capability_id))
        throw std::invalid_argument("Capability already registered: " +
                                    capability_id);
    if (handler && !entry->writable())
        throw std::invalid_argument("Handler requires a writable capability: " +
                                    capability_id);

    const bool initialize = !device().hasProperty(entry->key());
    if (!initialize && !entry->read(device()))
        throw std::invalid_argument("Existing property has the wrong type: " +
                                    capability_id);

    auto next = descriptor();
    next.addCapability(entry->descriptor);
    if (handler)
        actions_.emplace(capability_id, std::move(handler));
    try {
        if (initialize)
            entry->store(device(), entry->initial_value);
    } catch (...) {
        actions_.erase(capability_id);
        throw;
    }
    mutableDescriptor() = std::move(next);
}

void UniversalDeviceDriver::writeCapability(const std::string &capability_id,
                                            const Value &value) {
    const auto &entry = requireWritable(*this, capability_id);
    const auto action = actions_.find(capability_id);
    const auto &handler =
        action == actions_.end() ? transport_ : action->second;
    if (!handler) {
        entry.store(device(), value);
        return;
    }
    if (!entry.accepts(value))
        throw std::invalid_argument("Invalid value for capability: " +
                                    capability_id);
    handler(capability_id, value);
}

std::unique_ptr<IDeviceDriver>
createSimulatedDriver(const DeviceRecord &record) {
    if (record.driver_id != "simulated")
        throw std::invalid_argument(
            "Simulated creator requires the simulated protocol");
    return std::make_unique<UniversalDeviceDriver>(record);
}

} // namespace smart_home
