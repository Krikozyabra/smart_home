#pragma once

#include "CapabilityDescriptor.h"
#include <string>
#include <vector>
#include <utility>

namespace smart_home {

class DeviceDescriptor {
    unsigned int local_id;
    std::string physical_id;
    std::string name;
    std::vector<CapabilityDescriptor> capabilities;

  public:
    DeviceDescriptor(unsigned int c_local_id, std::string c_physical_id,
                     std::string c_name,
                     std::vector<CapabilityDescriptor> c_capabilities)
        : local_id(c_local_id), physical_id(std::move(c_physical_id)),
          name(std::move(c_name)), capabilities(std::move(c_capabilities)) {}

    unsigned int getLocalId() const {
        return this->local_id;
    }

    const std::string& getPhysicalId() const {
        return this->physical_id;
    }

    const std::string& getName() const {
        return this->name;
    }

    const std::vector<CapabilityDescriptor>& getCapabilities() const {
        return this->capabilities;
    }
};

} // namespace smart_home
