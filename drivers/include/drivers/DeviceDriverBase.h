/**
 * @file DeviceDriverBase.h
 * @brief Базовая реализация драйвера устройства умного дома.
 */

#pragma once

#include "descriptors/DeviceDescriptor.h"
#include "devices/Device.h"
#include "interfaces/IDeviceDriver.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace smart_home {

/**
 * @class DeviceDriverBase
 * @brief Базовый класс для всех драйверов устройств, реализующий интерфейс IDeviceDriver.
 */
class DeviceDriverBase : public IDeviceDriver {
  public:
    /**
     * @brief Возвращает ссылку на управляемый объект устройства.
     * @return Ссылка на объект устройства Device.
     */
    Device &device() override { return *this->device_object; }

    /**
     * @brief Возвращает дескриптор устройства.
     * @return Константная ссылка на дескриптор устройства DeviceDescriptor.
     */
    const DeviceDescriptor &descriptor() const override {
        return this->descriptor_object;
    }

  protected:
    /**
     * @brief Защищенный конструктор базового драйвера устройства.
     * @param c_device Уникальный указатель на объект устройства Device.
     * @param c_descriptor Дескриптор устройства DeviceDescriptor.
     * @throws std::invalid_argument Если c_device равен nullptr или если идентификатор
     *         устройства не совпадает с локальным идентификатором в дескрипторе.
     */
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
    /**
     * @brief Уникальный указатель на объект управляемого устройства.
     */
    std::unique_ptr<Device> device_object;

    /**
     * @brief Дескриптор устройства.
     */
    DeviceDescriptor descriptor_object;
};

} // namespace smart_home
