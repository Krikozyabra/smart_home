#pragma once

#include "persistence/PersistenceTypes.h"

#include <memory>

namespace smart_home::persistence {

/**
 * @brief Владеет соединением для свойств и ревизии рабочей SQLite-базы.
 * @details Инвентарь в таблице devices принадлежит storage. Обращения
 * к одному экземпляру и изменения инвентаря на время сохранения или
 * создания снимка сериализует вызывающий код.
 */
class DeviceStateStore {
  public:
    /**
     * @brief Открывает существующий файл SQLite для чтения и записи.
     * @throws std::runtime_error Ошибка открытия или конфигурации SQLite.
     */
    explicit DeviceStateStore(const std::filesystem::path &database_path);
    /** @brief Закрывает собственное соединение. */
    ~DeviceStateStore();

    /**
     * @brief Создаёт таблицы свойств и начальную ревизию 0 в чистой базе.
     * @pre Таблица инвентаря devices уже существует.
     * @throws std::runtime_error Ошибка SQLite или схема уже существует.
     */
    void initializeSchema();

    /**
     * @brief Проверяет целостность и читает типизированные свойства.
     * @details Поддерживается чтение прежней runtime_values из формата v1.
     * @throws std::runtime_error Ошибка схемы, ревизии, значений или чтения.
     */
    DevicePropertiesMap loadProperties(Revision expected_revision) const;

    /**
     * @brief Транзакционно заменяет полный набор свойств и обновляет ревизию.
     * @details updated_at содержит время сохранения в секундах Unix.
     * При первой записи прежняя runtime_values заменяется на device_properties
     * в той же транзакции. Ревизия может повторяться при повторном сохранении.
     * @throws std::runtime_error Неверные значения, ревизия или ошибка SQLite.
     */
    void storeProperties(const DevicePropertiesMap &properties,
                         Revision revision);

    /**
     * @brief Создаёт самостоятельную копию SQLite, включая инвентарь.
     * @param target Новый файл в существующем приватном каталоге на
     * tmpfs/ramfs.
     * @throws std::runtime_error Ошибка создания файла, backup или закрытия.
     */
    void backupTo(const std::filesystem::path &target) const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::persistence
