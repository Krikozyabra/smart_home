/**
 * @file DeviceDescriptor.h
 * @brief Описание дескриптора устройства умного дома.
 */

#pragma once

#include "CapabilityDescriptor.h"
#include "common/DeviceId.h"
#include <string>
#include <vector>
#include <utility>

namespace smart_home {

/**
 * @class DeviceDescriptor
 * @brief Описатель устройства, содержащий идентификаторы, имя и поддерживаемые возможности.
 */
class DeviceDescriptor {
    /**
     * @brief Локальный идентификатор устройства в системе.
     */
    DeviceId local_id;
    /**
     * @brief Физический (аппаратный) идентификатор устройства.
     */
    std::string physical_id;
    /**
     * @brief Пользовательское имя устройства.
     */
    std::string name;
    /**
     * @brief Список возможностей, поддерживаемых устройством.
     */
    std::vector<CapabilityDescriptor> capabilities;

  public:
    /**
     * @brief Конструктор описателя устройства.
     * @param c_local_id Локальный идентификатор устройства.
     * @param c_physical_id Физический идентификатор устройства.
     * @param c_name Имя устройства.
     * @param c_capabilities Список поддерживаемых возможностей устройства.
     */
    DeviceDescriptor(DeviceId c_local_id, std::string c_physical_id,
                     std::string c_name,
                     std::vector<CapabilityDescriptor> c_capabilities)
        : local_id(c_local_id), physical_id(std::move(c_physical_id)),
          name(std::move(c_name)), capabilities(std::move(c_capabilities)) {}

    /**
     * @brief Возвращает локальный идентификатор устройства.
     * @return Локальный идентификатор устройства.
     */
    DeviceId getLocalId() const {
        return this->local_id;
    }

    /**
     * @brief Возвращает физический идентификатор устройства.
     * @return Константная ссылка на строку с физическим идентификатором.
     */
    const std::string& getPhysicalId() const {
        return this->physical_id;
    }

    /**
     * @brief Возвращает имя устройства.
     * @return Константная ссылка на строку с именем устройства.
     */
    const std::string& getName() const {
        return this->name;
    }

    /**
     * @brief Возвращает список возможностей устройства.
     * @return Константная ссылка на вектор описателей возможностей.
     */
    const std::vector<CapabilityDescriptor>& getCapabilities() const {
        return this->capabilities;
    }
};

} // namespace smart_home
