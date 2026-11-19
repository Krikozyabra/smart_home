/** @file IOntologyStorage.h
 * @brief Интерфейс онтологии комнат, устройств, возможностей и связей.
 */
#pragma once
#include "ontology/OntologyRecords.h"
#include <optional>
namespace smart_home::ontology {
/** @brief Потокобезопасное хранилище. add* обновляет свойства существующего узла, сохраняя связи.
 * @details Все строковые поля и аргументы должны быть непустыми и не содержать NUL;
 * отрицательные DeviceId недопустимы, нулевой ключ допустим.
 * Устройство хранит только локальный ключ и название; его функции определяются
 * связями с каталогом возможностей, без статического поля типа.
 * Отсутствующий узел при get* возвращает nullopt, delete* возвращает false;
 * операции над связями требуют существования обоих концов.
 * После close() обращения, кроме повторного close(), выбрасывают std::logic_error.
 */
class IOntologyStorage {
public:
    /** @brief Освобождает реализацию без исключений. */
    virtual ~IOntologyStorage() = default;
    /** @brief Добавляет или обновляет комнату по ключу.
     * @param room Запись комнаты.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void addRoom(const RoomRecord& room) = 0;
    /** @brief Ищет комнату.
     * @param room_id Ключ комнаты.
     * @return Запись либо nullopt при отсутствии.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::optional<RoomRecord> getRoom(const std::string& room_id) const = 0;
    /** @brief Возвращает комнаты в порядке ключей.
     * @return Собственные копии записей.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::vector<RoomRecord> getAllRooms() const = 0;
    /** @brief Удаляет комнату и отвязывает её устройства.
     * @param room_id Ключ комнаты.
     * @return true, если узел был удалён.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual bool deleteRoom(const std::string& room_id) = 0;
    /** @brief Добавляет или обновляет устройство по локальному ключу.
     * @details Обновляет только название, сохраняя комнату и связи с возможностями.
     * @param device Локальный ключ и название устройства; наличие в инвентаре не проверяется.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void addDevice(const OntologyDeviceRecord& device) = 0;
    /** @brief Ищет устройство.
     * @param id Локальный ключ.
     * @return Запись либо nullopt при отсутствии.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::optional<OntologyDeviceRecord> getDevice(DeviceId id) const = 0;
    /** @brief Возвращает устройства в порядке ключей.
     * @return Собственные копии записей.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::vector<OntologyDeviceRecord> getAllDevices() const = 0;
    /** @brief Удаляет устройство и все его связи.
     * @param id Локальный ключ.
     * @return true, если узел был удалён.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual bool deleteDevice(DeviceId id) = 0;
    /** @brief Добавляет или обновляет возможность по ключу.
     * @param ability Запись возможности.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void addAbility(const AbilityRecord& ability) = 0;
    /** @brief Ищет возможность.
     * @param ability_id Ключ возможности.
     * @return Запись либо nullopt при отсутствии.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::optional<AbilityRecord> getAbility(const std::string& ability_id) const = 0;
    /** @brief Возвращает возможности в порядке ключей.
     * @return Собственные копии записей.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::vector<AbilityRecord> getAllAbilities() const = 0;
    /** @brief Удаляет возможность и её связи с устройствами.
     * @param ability_id Ключ возможности.
     * @return true, если узел был удалён.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual bool deleteAbility(const std::string& ability_id) = 0;
    /** @brief Атомарно назначает устройству единственную комнату.
     * @param device_id Ключ существующего устройства.
     * @param room_id Ключ существующей комнаты.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void assignDeviceToRoom(DeviceId device_id, const std::string& room_id) = 0;
    /** @brief Удаляет связь устройства с комнатой; повторный вызов допустим.
     * @param device_id Ключ существующего устройства.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void removeDeviceFromRoom(DeviceId device_id) = 0;
    /** @brief Ищет комнату существующего устройства.
     * @param device_id Ключ устройства.
     * @return Запись либо nullopt при отсутствии.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::optional<RoomRecord> getDeviceRoom(DeviceId device_id) const = 0;
    /** @brief Возвращает устройства существующей комнаты в порядке ключей.
     * @param room_id Ключ комнаты.
     * @return Собственные копии записей.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::vector<OntologyDeviceRecord> getDevicesInRoom(const std::string& room_id) const = 0;
    /** @brief Идемпотентно связывает устройство с возможностью.
     * @param device_id Ключ существующего устройства.
     * @param ability_id Ключ существующей возможности.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void addDeviceAbility(DeviceId device_id, const std::string& ability_id) = 0;
    /** @brief Удаляет связь; повторный вызов допустим.
     * @param device_id Ключ существующего устройства.
     * @param ability_id Ключ существующей возможности.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void removeDeviceAbility(DeviceId device_id, const std::string& ability_id) = 0;
    /** @brief Возвращает возможности существующего устройства в порядке ключей.
     * @param device_id Ключ устройства.
     * @return Собственные копии записей.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::vector<AbilityRecord> getDeviceAbilities(DeviceId device_id) const = 0;
    /** @brief Возвращает устройства с существующей возможностью в порядке ключей.
     * @details Выборка по связям HAS_FEATURE; устройство может входить в выборки
     * нескольких возможностей. Отсутствие связей даёт пустой список.
     * @param ability_id Ключ возможности.
     * @return Собственные копии записей.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::vector<OntologyDeviceRecord> getDevicesWithAbility(const std::string& ability_id) const = 0;
    /** @brief Возвращает согласованный снимок устройства и связей.
     * @details Результат содержит локальный ключ, название, ключ комнаты (пустой
     * при отсутствии назначения) и отсортированные ключи возможностей.
     * @param device_id Ключ устройства.
     * @return Запись либо nullopt при отсутствии.
     * @throws std::invalid_argument Некорректные данные или отсутствующие концы связи.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual std::optional<DeviceOntologyDetails> getDeviceDetails(DeviceId device_id) const = 0;
    /** @brief Синхронно сохраняет согласованный атомарный снимок в указанный файл.
     * @details Блокирует запросы на время CHECKPOINT, копирования и повторного открытия.
     * Не меняет database_path и dump_path. До rename прежний файл сохраняется;
     * после rename ошибка сообщает о неопределённости долговечности замены.
     * Родитель должен существовать; вызывающий обеспечивает исключительный доступ
     * к назначению без открытого движка и внешних sidecar-файлов.
     * @param path Непустой путь без NUL; обычный файл или новое имя.
     * @throws std::invalid_argument Неверный путь, ссылка, рабочий файл или активный lockfile.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     * @throws std::logic_error Хранилище закрыто или движок недоступен после сбоя.
     */
    virtual void dumpToFile(const std::string& path) = 0;
    /** @brief Сохраняет dump_path, если задан, и освобождает ресурсы; повторный вызов допустим.
     * @details При ошибке автодампа ресурсы сохраняются; close() можно повторить.
     * @throws std::runtime_error Ошибка движка или файловой системы.
     */
    virtual void close() = 0;
};
} // namespace smart_home::ontology
