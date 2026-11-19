#include "devices/Device.h"

#include <utility>

namespace smart_home {
namespace {

void requirePropertyKey(const std::string &key) {
    if (key.empty())
        throw std::invalid_argument("Property key must not be empty");
}

} // namespace

Device::Device(DeviceId id, std::string name)
    : id_(id), name_(std::move(name)) {}

DeviceId Device::getId() const noexcept { return id_; }

std::string Device::getName() const { return name_; }

void Device::setName(std::string name) {
    if (name.empty())
        throw std::invalid_argument("Device name must not be empty");
    name_ = std::move(name);
}

void Device::setProperty(const std::string &key, Value value) {
    requirePropertyKey(key);
    properties_.insert_or_assign(key, std::move(value));
}

std::optional<Value> Device::getProperty(const std::string &key) const {
    if (const auto *value = findProperty(key))
        return *value;
    return std::nullopt;
}

bool Device::hasProperty(const std::string &key) const {
    return findProperty(key) != nullptr;
}

bool Device::removeProperty(const std::string &key) {
    requirePropertyKey(key);
    return properties_.erase(key) != 0;
}

const std::unordered_map<std::string, Value> &
Device::getProperties() const noexcept {
    return properties_;
}

const Value *Device::findProperty(const std::string &key) const {
    requirePropertyKey(key);
    const auto property = properties_.find(key);
    return property == properties_.end() ? nullptr : &property->second;
}

} // namespace smart_home
