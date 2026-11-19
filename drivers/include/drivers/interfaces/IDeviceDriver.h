#pragma once

/**
 * @file IDeviceDriver.h
 * @brief Интерфейс драйвера устройства умного дома.
 */

#include "devices/Device.h"

#include <descriptors/DeviceDescriptor.h>
#include <string>

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

    /**
     * @brief Выполняет запись в поддерживаемую стандартную возможность.
     * @param capability_id Идентификатор возможности.
     * @param value Проверяемое значение из команды.
     * @details Базовая реализация записывает типизированное значение в Device.
     * Вызывающий код сериализует доступ к драйверу, устройству и дескриптору.
     * @throws std::invalid_argument Операция не поддерживается или значение неверно.
     */
    virtual void writeCapability(const std::string &capability_id,
                                 const Value &value);
};

}
