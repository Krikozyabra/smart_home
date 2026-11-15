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
 * @details Объект владеет соединением. Обращения сериализует вызывающий код.
 * После успешного close() все операции, кроме повторного close(), выбрасывают
 * std::logic_error до проверки аргументов и обращения к SQLite или файлам.
 */
class SQLiteDeviceStorage final : public IDeviceStorage {
  private:
    /**
     * @brief Дескриптор рабочей базы; nullptr означает закрытое состояние.
     */
    sqlite3* database = nullptr;
    std::string dump_path;

    /** @brief Проверяет открытое состояние; иначе выбрасывает std::logic_error. */
    void ensureOpen() const;

    /**
     * @brief Создает схему таблиц базы данных SQLite, если она еще не создана.
     * @throws std::runtime_error При ошибке создания таблицы.
     */
    void createSchema();

  public:
    /**
     * @brief Инициализирует хранилище и открывает соединение с базой данных SQLite по указанному пути.
     * @param database_path Путь к рабочей базе или :memory: для базы в RAM.
     * @param dump_path Назначение финального атомарного dump; пустой отключает его.
     * Относительный путь фиксируется относительно текущего каталога при создании.
     * @throws std::invalid_argument Недопустимое назначение dump (см. dumpToFile()).
     * @throws std::runtime_error При ошибке открытия базы или создания схемы.
     */
    explicit SQLiteDeviceStorage(const std::string &database_path,
                                 const std::string &dump_path = {});

    /**
     * @brief Вызывает close() как безопасный резервный путь завершения.
     * @details Ошибка dump однократно сообщается через logging и не выходит
     * исключением. Все обращения к объекту должны завершиться до уничтожения;
     * исключительный доступ к назначению требуется до окончания деструктора.
     * После успешного явного close() повторного dump нет. При ошибке резервного
     * close() соединение всё равно освобождается.
     */
    ~SQLiteDeviceStorage() noexcept override;

    /**
     * @brief Выполняет настроенный финальный dump и закрывает соединение.
     * @details После успеха повторный вызов ничего не делает. При ошибке dump
     * соединение остаётся открытым: данные доступны, close() можно повторить.
     * Ошибка передаётся вызывающему коду без логирования. Перед вызовом нужно
     * завершить остальные обращения и обеспечить исключительный доступ к dump.
     * @throws std::runtime_error Ошибка финального dump или закрытия соединения.
     */
    void close() override;

    /**
     * @brief Синхронно публикует всю базу main в самостоятельный SQLite-файл.
     * @param path Путь назначения; родительский каталог должен существовать.
     * @details Сохраняет схему, все таблицы, ID и AUTOINCREMENT через backup API,
     * затем выполняет fsync файла, атомарный rename и fsync каталога (Linux).
     * Рабочая база и настроенный путь автодампа не меняются. Вызывающий код
     * сериализует обращения к источнику и обеспечивает исключительный доступ
     * к назначению без открытых соединений и внешних WAL/journal-файлов.
     * @throws std::invalid_argument Путь пуст, содержит NUL, указывает на ссылку,
     * не обычный файл или рабочую базу, в том числе через файловый псевдоним.
     * @throws std::runtime_error Ошибка этапа сохранения. До rename прежний файл
     * неизменен; после rename ошибка fsync каталога означает неопределённость
     * долговечности замены. Источник остаётся доступен, dump можно повторить.
     */
    void dumpToFile(const std::string &path) override;

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
     * @param driver_id Идентификатор драйвера.
     * @return Объект std::optional с найденной записью DeviceRecord или std::nullopt, если запись отсутствует.
     * @throws std::invalid_argument Если один из идентификаторов пуст.
     * @throws std::runtime_error При ошибке SQLite или неожиданном NULL в записи.
     */
    std::optional<DeviceRecord>
    findByPhysicalId(const std::string &physical_id,
                     const std::string &driver_id) const override;

    /**
     * @brief Вставляет новую запись об устройстве в таблицу базы данных SQLite.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера.
     * @param default_name Имя устройства по умолчанию.
     * @return Созданная запись DeviceRecord с присвоенным локальным идентификатором.
     * @throws std::invalid_argument Если идентификаторы пусты или их пара уже присутствует.
     * @throws std::runtime_error При ошибке SQLite или отрицательном локальном идентификаторе.
     */
    DeviceRecord insert(const std::string &physical_id,
                        const std::string &driver_id,
                        const std::string &default_name) override;

    /**
     * @brief Извлекает все записи об устройствах из базы данных SQLite.
     * @return Вектор всех сохраненных записей DeviceRecord.
     * @throws std::runtime_error При ошибке SQLite или неожиданном NULL в записи.
     */
    std::vector<DeviceRecord> getAll() const override;
};

} // namespace smart_home
