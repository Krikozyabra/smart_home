#pragma once

/**
 * @file IDeviceDriver.h
 * @brief Интерфейс драйвера устройства умного дома.
 */

#include "devices/Device.h"

#include <descriptors/DeviceDescriptor.h>

namespace smart_home{

/**
 * @class IDeviceDriver
 * @brief Абстрактный интерфейс драйвера устройства.
 */
class IDeviceDriver{
public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IDeviceDriver() = default;

    /**
     * @brief Предоставляет доступ к управляемому устройству.
     * @return Ссылка на объект Device.
     */
    virtual Device& device() = 0;

    /**
     * @brief Возвращает дескриптор устройства с метаданными.
     * @return Константная ссылка на дескриптор DeviceDescriptor.
     */
    virtual const DeviceDescriptor& descriptor() const = 0;
};

}
