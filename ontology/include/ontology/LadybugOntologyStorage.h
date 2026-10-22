/** @file LadybugOntologyStorage.h
 * @brief LadybugDB: рабочая RAM-копия и атомарные постоянные снимки.
 */
#pragma once
#include "ontology/IOntologyStorage.h"
#include "ontology/OntologyConfig.h"
#include <memory>
namespace smart_home::ontology {
namespace detail { struct OntologyTestAccess; }
/** @brief Единственный владелец базы, блокировки и фонового потока.
 * @details close() сообщает об ошибке финального сохранения; при ошибке можно повторить close().
 * Деструктор подавляет исключения и при неудаче сохраняет рабочий каталог для ручного восстановления.
 * Периодический сбой логируется, следующая попытка выполняется через заданный период.
 */
class LadybugOntologyStorage final : public IOntologyStorage {
public:
    /** @brief Открывает базу и запускает синхронизацию.
     * @param config Настройки.
     * @throws std::invalid_argument Некорректные параметры.
     * @throws std::runtime_error База занята, повреждена, несовместима либо недоступны файлы/RAM.
     */
    explicit LadybugOntologyStorage(OntologyConfig config);
    /** @brief Пытается сохранить базу; исключения подавляются. */
    ~LadybugOntologyStorage() noexcept override;
    LadybugOntologyStorage(const LadybugOntologyStorage&) = delete; ///< Копирование запрещено.
    LadybugOntologyStorage& operator=(const LadybugOntologyStorage&) = delete; ///< Присваивание запрещено.
    /** @copydoc IOntologyStorage::addRoom */
    void addRoom(const RoomRecord& room) override;
    /** @copydoc IOntologyStorage::getRoom */
    std::optional<RoomRecord> getRoom(const std::string& room_id) const override;
    /** @copydoc IOntologyStorage::getAllRooms */
    std::vector<RoomRecord> getAllRooms() const override;
    /** @copydoc IOntologyStorage::deleteRoom */
    bool deleteRoom(const std::string& room_id) override;
    /** @copydoc IOntologyStorage::addDevice */
    void addDevice(const OntologyDeviceRecord& device) override;
    /** @copydoc IOntologyStorage::getDevice */
    std::optional<OntologyDeviceRecord> getDevice(DeviceId id) const override;
    /** @copydoc IOntologyStorage::getAllDevices */
    std::vector<OntologyDeviceRecord> getAllDevices() const override;
    /** @copydoc IOntologyStorage::deleteDevice */
    bool deleteDevice(DeviceId id) override;
    /** @copydoc IOntologyStorage::addAbility */
    void addAbility(const AbilityRecord& ability) override;
    /** @copydoc IOntologyStorage::getAbility */
    std::optional<AbilityRecord> getAbility(const std::string& ability_id) const override;
    /** @copydoc IOntologyStorage::getAllAbilities */
    std::vector<AbilityRecord> getAllAbilities() const override;
    /** @copydoc IOntologyStorage::deleteAbility */
    bool deleteAbility(const std::string& ability_id) override;
    /** @copydoc IOntologyStorage::assignDeviceToRoom */
    void assignDeviceToRoom(DeviceId device_id, const std::string& room_id) override;
    /** @copydoc IOntologyStorage::removeDeviceFromRoom */
    void removeDeviceFromRoom(DeviceId device_id) override;
    /** @copydoc IOntologyStorage::getDeviceRoom */
    std::optional<RoomRecord> getDeviceRoom(DeviceId device_id) const override;
    /** @copydoc IOntologyStorage::getDevicesInRoom */
    std::vector<OntologyDeviceRecord> getDevicesInRoom(const std::string& room_id) const override;
    /** @copydoc IOntologyStorage::addDeviceAbility */
    void addDeviceAbility(DeviceId device_id, const std::string& ability_id) override;
    /** @copydoc IOntologyStorage::removeDeviceAbility */
    void removeDeviceAbility(DeviceId device_id, const std::string& ability_id) override;
    /** @copydoc IOntologyStorage::getDeviceAbilities */
    std::vector<AbilityRecord> getDeviceAbilities(DeviceId device_id) const override;
    /** @copydoc IOntologyStorage::getDevicesWithAbility */
    std::vector<OntologyDeviceRecord> getDevicesWithAbility(const std::string& ability_id) const override;
    /** @copydoc IOntologyStorage::getDeviceDetails */
    std::optional<DeviceOntologyDetails> getDeviceDetails(DeviceId device_id) const override;
    /** @copydoc IOntologyStorage::sync */
    void sync() override;
    /** @copydoc IOntologyStorage::close */
    void close() override;
private:
    friend struct detail::OntologyTestAccess;
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace smart_home::ontology
