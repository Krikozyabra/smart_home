/**
 * @file DeviceRecord.h
 * @brief Определение структуры записи об устройстве в системе умного дома.
 */

#pragma once

#include "common/DeviceId.h"

#include <string>

namespace smart_home {

/**
 * @struct DeviceRecord
 * @brief Запись с данными устройства, хранящаяся в системе.
 */
struct DeviceRecord {
    /**
     * @brief Локальный целочисленный идентификатор устройства.
     */
    DeviceId local_id;

    /**
     * @brief Физический (аппаратный) идентификатор устройства.
     */
    std::string physical_id;

    /**
     * @brief Идентификатор драйвера, управляющего данным устройством.
     */
    std::string driver_id;

    /**
     * @brief Пользовательское или назначенное имя устройства.
     */
    std::string name;

    /**
     * @brief Оператор сравнения двух записей устройств на равенство.
     * @param other Другая запись DeviceRecord для сравнения.
     * @return true, если все поля записей совпадают, иначе false.
     */
    inline bool operator==(const DeviceRecord &other) const noexcept {
        return local_id == other.local_id && physical_id == other.physical_id &&
               driver_id == other.driver_id && name == other.name;
    }
};

} // namespace smart_home
