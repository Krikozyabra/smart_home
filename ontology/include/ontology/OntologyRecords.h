/** @file OntologyRecords.h
 * @brief Собственные типы записей и результатов онтологии; не зависят от SDK.
 */
#pragma once
#include "common/DeviceId.h"
#include <string>
#include <vector>
namespace smart_home::ontology {
/** @brief Комната с постоянным строковым ключом. */
struct RoomRecord {
    std::string id; ///< Ключ комнаты.
    std::string name; ///< Название комнаты.
    /** @brief Сравнивает все поля. @param other Другая запись. @return Равенство записей. */
    bool operator==(const RoomRecord& other) const noexcept { return id == other.id && name == other.name; }
};
/** @brief Устройство; ключ совпадает с локальным DeviceId реестра. */
struct OntologyDeviceRecord {
    DeviceId id; ///< Локальный ключ.
    std::string name; ///< Название.
    std::string type; ///< Тип устройства.
    /** @brief Сравнивает все поля. @param other Другая запись. @return Равенство записей. */
    bool operator==(const OntologyDeviceRecord& other) const noexcept { return id == other.id && name == other.name && type == other.type; }
};
/** @brief Возможность с ключом CapabilityDescriptor::getId(). */
struct AbilityRecord {
    std::string id; ///< Ключ возможности.
    std::string name; ///< Название.
    std::string category; ///< Категория.
    /** @brief Сравнивает все поля. @param other Другая запись. @return Равенство записей. */
    bool operator==(const AbilityRecord& other) const noexcept { return id == other.id && name == other.name && category == other.category; }
};
/** @brief Согласованный снимок устройства и его связей. */
struct DeviceOntologyDetails {
    OntologyDeviceRecord device; ///< Запись устройства.
    std::string room_id; ///< Ключ комнаты; пустой при отсутствии связи.
    std::vector<std::string> ability_ids; ///< Отсортированные ключи возможностей.
};
} // namespace smart_home::ontology
