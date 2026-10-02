/**
 * @file IDeviceStorage.h
 * @brief Интерфейс хранилища записей устройств.
 */

#pragma once

#include "storage/DeviceRecord.h"

#include <optional>
#include <string>
#include <vector>

namespace smart_home {

/**
 * @class IDeviceStorage
 * @brief Интерфейс для управления постоянным хранением записей устройств.
 */
class IDeviceStorage {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IDeviceStorage() = default;

    /**
     * @brief Поиск записи устройства по физическому идентификатору и идентификатору драйвера.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @return Запись устройства DeviceRecord, если найдена; иначе std::nullopt.
     */
    virtual std::optional<DeviceRecord>
    findByPhysicalId(const std::string &physical_id,
                     const std::string &driver_id) const = 0;

    /**
     * @brief Добавление новой записи устройства в хранилище.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @param name Название устройства.
     * @return Созданная запись устройства DeviceRecord.
     */
    virtual DeviceRecord insert(const std::string &physical_id,
                                const std::string &driver_id,
                                const std::string &name) = 0;

    /**
     * @brief Получение всех записей устройств из хранилища.
     * @return Список всех сохраненных записей устройств.
     */
    virtual std::vector<DeviceRecord> getAll() const = 0;
};

} // namespace smart_home
