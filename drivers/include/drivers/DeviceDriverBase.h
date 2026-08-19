#pragma once

#include "DeviceDescriptor.h"
#include "devices/Device.h"
#include "interfaces/IDeviceDriver.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace smart_home {

class DeviceDriverBase : public IDeviceDriver {
  public:
    Device &device() override { return *this->device_object; }

    const DeviceDescriptor &descriptor() const override {
        return this->descriptor_object;
    }

  protected:
    DeviceDriverBase(std::unique_ptr<Device> c_device,
                     DeviceDescriptor c_descriptor)
        : device_object(std::move(c_device)),
          descriptor_object(std::move(c_descriptor)) {
        if (device_object == nullptr)
            throw std::invalid_argument("Device must not be null");

        if (device_object->getId() != descriptor_object.getLocalId())
            throw std::invalid_argument(
                "Device and descriptor IDs do not match");
    }

  private:
    std::unique_ptr<Device> device_object;
    DeviceDescriptor descriptor_object;
};

} // namespace smart_home
