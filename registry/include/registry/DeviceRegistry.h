/**
 * @file DeviceRegistry.h
 * @brief Реестр активных драйверов устройств системы умного дома.
 */

#pragma once

#include "common/DeviceId.h"
#include "drivers/interfaces/IDeviceDriver.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace smart_home {

/**
 * @class DeviceRegistry
 * @brief Реестр для хранения и поиска активных драйверов устройств по их идентификаторам.
 */
class DeviceRegistry {
  private:
    /**
     * @brief Хранилище драйверов устройств, сопоставляющее идентификатор устройства DeviceId с уникальным указателем на драйвер IDeviceDriver.
     */
    std::unordered_map<DeviceId, std::unique_ptr<IDeviceDriver>> drivers;

  public:
    /**
     * @brief Добавляет новый драйвер устройства в реестр.
     *
     * Первый параметр передает уникальный указатель на регистрируемый драйвер IDeviceDriver.
     * @throws std::invalid_argument Если передан nullptr.
     * @throws std::runtime_error Если драйвер с этим локальным идентификатором уже зарегистрирован.
     *
     */
    void add(std::unique_ptr<IDeviceDriver>);

    /**
     * @brief Выполняет поиск драйвера устройства по его идентификатору.
     *
     * Первый параметр задает идентификатор искомого устройства DeviceId.
     *
     * @return Указатель на найденный драйвер IDeviceDriver или nullptr, если драйвер не найден.
     */
    IDeviceDriver *find(DeviceId);

    /**
     * @brief Выполняет поиск драйвера устройства по его идентификатору (константная версия).
     *
     * Первый параметр задает идентификатор искомого устройства DeviceId.
     *
     * @return Константный указатель на найденный драйвер IDeviceDriver или nullptr, если драйвер не найден.
     */
    const IDeviceDriver *find(DeviceId) const;

    /**
     * @brief Возвращает текущее количество зарегистрированных драйверов устройств.
     * @return Количество драйверов в реестре.
     */
    std::size_t size() const;
};

} // namespace smart_home
