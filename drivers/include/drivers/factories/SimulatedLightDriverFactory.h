#pragma once

/**
 * @file SimulatedLightDriverFactory.h
 * @brief Фабрика для создания драйверов симулированного освещения.
 */

#include "IDeviceDriverFactory.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "storage/DeviceRecord.h"

#include <memory>
#include <string>

namespace smart_home {

/**
 * @class SimulatedLightDriverFactory
 * @brief Фабрика для создания экземпляров драйвера симулированного освещения.
 */
class SimulatedLightDriverFactory final : public IDeviceDriverFactory {
  public:
    /**
     * @brief Возвращает строковый идентификатор драйвера симулированного освещения.
     * @return Идентификатор драйвера (std::string).
     */
    std::string getDriverId() const override;

    /**
     * @brief Создает экземпляр драйвера симулированного освещения.
     * Первый параметр — запись об устройстве (const DeviceRecord &).
     * @return Умный указатель std::unique_ptr на созданный драйвер IDeviceDriver.
     */
    std::unique_ptr<IDeviceDriver> create(const DeviceRecord &) const override;
};

} // namespace smart_home
