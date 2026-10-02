/**
 * @file DriverFactoryRegistry.h
 * @brief Реестр фабрик драйверов устройств системы умного дома.
 */

#pragma once

#include "drivers/factories/IDeviceDriverFactory.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
namespace smart_home {

/**
 * @class DriverFactoryRegistry
 * @brief Реестр для регистрации и поиска фабрик драйверов устройств по их строковым идентификаторам.
 */
class DriverFactoryRegistry {
  private:
    /**
     * @brief Хранилище зарегистрированных фабрик драйверов, сопоставляющее строковый идентификатор драйвера с уникальным указателем на фабрику IDeviceDriverFactory.
     */
    std::unordered_map<std::string, std::unique_ptr<IDeviceDriverFactory>>
        factories;

  public:
    /**
     * @brief Регистрирует новую фабрику драйверов устройств в реестре.
     *
     * Первый параметр передает уникальный указатель на регистрируемую фабрику IDeviceDriverFactory.
     * @throws std::invalid_argument Если передан nullptr или идентификатор драйвера пуст.
     * @throws std::runtime_error Если фабрика для этого драйвера уже зарегистрирована.
     *
     */
    void add(std::unique_ptr<IDeviceDriverFactory>);

    /**
     * @brief Выполняет поиск фабрики драйверов по идентификатору драйвера.
     * @param driver_id Строковый идентификатор драйвера.
     * @return Указатель на найденную фабрику IDeviceDriverFactory или nullptr, если фабрика не найдена.
     */
    IDeviceDriverFactory *find(const std::string &driver_id);

    /**
     * @brief Выполняет поиск фабрики драйверов по идентификатору драйвера (константная версия).
     * @param driver_id Строковый идентификатор драйвера.
     * @return Константный указатель на найденную фабрику IDeviceDriverFactory или nullptr, если фабрика не найдена.
     */
    const IDeviceDriverFactory *find(const std::string &driver_id) const;

    /**
     * @brief Возвращает текущее количество зарегистрированных фабрик драйверов.
     * @return Количество фабрик в реестре.
     */
    std::size_t size() const noexcept;
};

} // namespace smart_home
