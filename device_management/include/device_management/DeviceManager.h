/**
 * @file DeviceManager.h
 * @brief Определение менеджера устройств и связанных классов исключений.
 */

#pragma once

#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <stdexcept>
#include <string>

namespace smart_home {

/**
 * @class UnsupportedDriverError
 * @brief Исключение, выбрасываемое при попытке использовать неподдерживаемый драйвер устройства.
 */
class UnsupportedDriverError : public std::invalid_argument {
  public:
    /**
     * @brief Конструктор исключения неподдерживаемого драйвера.
     * @param message Описание ошибки.
     */
    explicit UnsupportedDriverError(const std::string &message)
        : std::invalid_argument(message) {}
};

/**
 * @class DeviceManager
 * @brief Менеджер управления устройствами, координирующий их сохранение, обнаружение и регистрацию.
 */
class DeviceManager {
  private:
    /**
     * @brief Ссылка на хранилище записей об устройствах.
     */
    IDeviceStorage &storage;
    /**
     * @brief Ссылка на реестр фабрик драйверов.
     */
    const DriverFactoryRegistry &factory_registry;
    /**
     * @brief Ссылка на реестр активных устройств.
     */
    DeviceRegistry &device_registry;

    /**
     * @brief Получает существующую запись устройства или создает новую при ее отсутствии.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @param default_name Имя устройства по умолчанию.
     * @return Запись об устройстве DeviceRecord.
     */
    DeviceRecord getOrCreate(const std::string &physical_id,
                             const std::string &driver_id,
                             const std::string &default_name);

  public:
    /**
     * @brief Конструктор менеджера устройств.
     * @param storage Ссылка на хранилище данных об устройствах.
     * @param factory_registry Ссылка на реестр фабрик драйверов устройств.
     * @param device_registry Ссылка на реестр зарегистрированных устройств.
     */
    DeviceManager(IDeviceStorage &storage,
                  const DriverFactoryRegistry &factory_registry,
                  DeviceRegistry &device_registry);

    /**
     * @brief Восстанавливает ранее сохраненные устройства из хранилища.
     * @return Количество успешно восстановленных устройств.
     * @throws std::runtime_error Если фабрика сохраненного драйвера не найдена.
     * @throws std::logic_error Если фабрика вернула nullptr.
     */
    std::size_t restoreStoredDevices();

    /**
     * @brief Регистрирует вновь обнаруженное устройство.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @param default_name Имя устройства по умолчанию.
     * @return Запись об обнаруженном устройстве DeviceRecord.
     * @throws UnsupportedDriverError Если фабрика указанного драйвера не найдена.
     * @throws std::logic_error Если фабрика вернула nullptr.
     */
    DeviceRecord registerDiscoveredDevice(const std::string &physical_id,
                                          const std::string &driver_id,
                                          const std::string &default_name);
};

} // namespace smart_home
