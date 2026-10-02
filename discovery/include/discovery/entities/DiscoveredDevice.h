/**
 * @file DiscoveredDevice.h
 * @brief Описание структуры обнаруженного устройства.
 */

#pragma once

#include <string>
namespace smart_home {

/**
 * @brief Структура, содержащая информацию об обнаруженном устройстве.
 */
struct DiscoveredDevice {
    /**
     * @brief Физический идентификатор устройства.
     */
    std::string physical_id;

    /**
     * @brief Идентификатор совместимого драйвера устройства.
     */
    std::string driver_id;

    /**
     * @brief Имя устройства по умолчанию.
     */
    std::string default_name;

    /**
     * @brief Оператор сравнения на равенство двух обнаруженных устройств.
     * @param right Правый операнд для сравнения.
     * @return true, если все поля совпадают, иначе false.
     */
    inline bool operator==(const DiscoveredDevice &right) const{
        return (physical_id == right.physical_id) &&
               (driver_id == right.driver_id) &&
               (default_name == right.default_name);
    }
};

} // namespace smart_home
