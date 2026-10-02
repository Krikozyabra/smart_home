#pragma once

/**
 * @file IDeviceDriverFactory.h
 * @brief Интерфейс фабрики для создания драйверов устройств.
 */

#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <string>
namespace smart_home {

/**
 * @class IDeviceDriverFactory
 * @brief Интерфейс фабрики создания драйверов устройств.
 */
class IDeviceDriverFactory {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IDeviceDriverFactory() = default;

    /**
     * @brief Возвращает строковый идентификатор драйвера.
     * @return Идентификатор типа драйвера (std::string).
     */
    virtual std::string getDriverId() const = 0;

    /**
     * @brief Создает экземпляр драйвера устройства на основе конфигурационной записи.
     * Первый параметр — запись об устройстве (const DeviceRecord &).
     * @return Умный указатель std::unique_ptr на созданный драйвер IDeviceDriver.
     */
    virtual std::unique_ptr<IDeviceDriver> create(const DeviceRecord &) const = 0;
};

} // namespace smart_home
