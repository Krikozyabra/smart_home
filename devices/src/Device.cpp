#include "devices/Device.h"

#include <string>
#include <utility>

namespace smart_home {

Device::Device(unsigned int c_id, std::string c_name) : id(c_id), name(std::move(c_name)) {}

std::string Device::getName() const { return this->name; }
void Device::setName(std::string new_name) { this->name = std::move(new_name); }

unsigned int Device::getId() const { return this->id; }

}
