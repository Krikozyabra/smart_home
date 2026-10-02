/**
 * @file SQLiteDeviceStorage.h
 * @brief Реализация хранилища устройств на базе базы данных SQLite.
 */

#pragma once

#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"
#include "sqlite3.h"

#include <optional>
#include <string>
#include <vector>

namespace smart_home {

/**
 * @class SQLiteDeviceStorage
 * @brief Хранилище устройств на основе SQLite, реализующее интерфейс IDeviceStorage.
 */
class SQLiteDeviceStorage final : public IDeviceStorage {
  private:
    /**
     * @brief Указатель на структуру дескриптора базы данных SQLite.
     */
    sqlite3* database = nullptr;

    /**
     * @brief Создает схему таблиц базы данных SQLite, если она еще не создана.
     * @throws std::runtime_error При ошибке создания таблицы.
     */
    void createSchema();

  public:
    /**
     * @brief Инициализирует хранилище и открывает соединение с базой данных SQLite по указанному пути.
     * @param database_path Путь к файлу базы данных SQLite на диске.
     * @throws std::runtime_error При ошибке открытия базы или создания схемы.
     */
    explicit SQLiteDeviceStorage(const std::string &database_path);

    /**
     * @brief Деструктор, закрывающий дескриптор базы данных SQLite и освобождающий занятые ресурсы.
     */
    ~SQLiteDeviceStorage() override;

    /**
     * @brief Конструктор копирования удален для предотвращения дублирования дескриптора базы данных.
     *
     * Первый параметр передает копируемый объект SQLiteDeviceStorage.
     *
     */
    SQLiteDeviceStorage(const SQLiteDeviceStorage &) = delete;

    /**
     * @brief Оператор присваивания копированием удален для предотвращения дублирования дескриптора базы данных.
     *
     * Первый параметр передает копируемый объект SQLiteDeviceStorage.
     *
     * @return Ссылка на текущий объект SQLiteDeviceStorage.
     */
    SQLiteDeviceStorage &operator=(const SQLiteDeviceStorage &) = delete;

    /**
     * @brief Выполняет поиск записи устройства по физическому идентификатору и идентификатору драйвера.
     * @param physical_id Физический идентификатор устройства.
     * @param device_id Идентификатор драйвера (параметр назван device_id в интерфейсе).
     * @return Объект std::optional с найденной записью DeviceRecord или std::nullopt, если запись отсутствует.
     * @throws std::invalid_argument Если один из идентификаторов пуст.
     * @throws std::runtime_error При ошибке SQLite или неожиданном NULL в записи.
     */
    std::optional<DeviceRecord>
    findByPhysicalId(const std::string &physical_id,
                     const std::string &device_id) const override;

    /**
     * @brief Вставляет новую запись об устройстве в таблицу базы данных SQLite.
     * @param physical_id Физический идентификатор устройства.
     * @param device_id Идентификатор драйвера (параметр назван device_id в интерфейсе).
     * @param default_name Имя устройства по умолчанию.
     * @return Созданная запись DeviceRecord с присвоенным локальным идентификатором.
     * @throws std::invalid_argument Если идентификаторы пусты или их пара уже присутствует.
     * @throws std::runtime_error При ошибке SQLite или отрицательном локальном идентификаторе.
     */
    DeviceRecord insert(const std::string &physical_id,
                        const std::string &device_id,
                        const std::string &default_name) override;

    /**
     * @brief Извлекает все записи об устройствах из базы данных SQLite.
     * @return Вектор всех сохраненных записей DeviceRecord.
     * @throws std::runtime_error При ошибке SQLite или неожиданном NULL в записи.
     */
    std::vector<DeviceRecord> getAll() const override;
};

} // namespace smart_home
