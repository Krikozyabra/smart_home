/**
 * @file InMemoryDeviceStorage.h
 * @brief Реализация хранилища устройств в оперативной памяти.
 */

#pragma once

#include "common/DeviceId.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

/**
 * @class InMemoryDeviceStorage
 * @brief Хранилище устройств в оперативной памяти, реализующее интерфейс IDeviceStorage.
 */
class InMemoryDeviceStorage final : public IDeviceStorage {
  private:
    /**
     * @brief Составной ключ устройства, объединяющий физический идентификатор и идентификатор драйвера.
     */
    using DeviceKey = std::pair<std::string, std::string>;

    /**
     * @brief Отображение составных ключей DeviceKey на записи DeviceRecord, хранящиеся в памяти.
     */
    std::map<DeviceKey, DeviceRecord> records;

    /**
     * @brief Следующий доступный локальный идентификатор для нового устройства.
     */
    DeviceId next_local_id = 0;

  public:
    /**
     * @brief Вставляет новую запись об устройстве в хранилище.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @param name Название устройства.
     * @return Созданная запись DeviceRecord с присвоенным локальным идентификатором.
     * @throws std::invalid_argument Если идентификаторы пусты или их пара уже присутствует.
     */
    DeviceRecord insert(const std::string &physical_id,
                        const std::string &driver_id,
                        const std::string &name) override;

    /**
     * @brief Выполняет поиск записи устройства по физическому идентификатору и идентификатору драйвера.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @return Объект std::optional с найденной записью DeviceRecord или std::nullopt, если запись не найдена.
     * @throws std::invalid_argument Если физический идентификатор или идентификатор драйвера пуст.
     */
    std::optional<DeviceRecord>
    findByPhysicalId(const std::string &physical_id,
                     const std::string &driver_id) const override;

    /**
     * @brief Возвращает все записи об устройствах, хранящиеся в оперативной памяти.
     * @return Вектор всех записей DeviceRecord.
     */
    std::vector<DeviceRecord> getAll() const override;
};

} // namespace smart_home
