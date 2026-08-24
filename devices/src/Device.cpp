#include "devices/Device.h"
#include "common/DeviceId.h"

#include <string>
#include <utility>

namespace smart_home {

Device::Device(DeviceId c_id, std::string c_name)
    : id(c_id), name(std::move(c_name)) {}

std::string Device::getName() const { return this->name; }
void Device::setName(std::string new_name) { this->name = std::move(new_name); }

DeviceId Device::getId() const { return this->id; }

} // namespace smart_home
