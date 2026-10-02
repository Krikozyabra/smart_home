/**
 * @file Device.h
 * @brief Базовый класс устройства умного дома.
 */

#pragma once

#include "common/DeviceId.h"
#include <string>

namespace smart_home {

/**
 * @class Device
 * @brief Базовый класс устройства в системе умного дома.
 */
class Device {
  public:
    /**
     * @brief Конструктор базового устройства.
     *
     * Первый параметр (позиция 1) задает локальный идентификатор устройства DeviceId.
     * Второй параметр (позиция 2) задает имя устройства std::string.
     */
    Device(DeviceId, std::string);
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~Device() = default;

    /**
     * @brief Возвращает идентификатор устройства.
     * @return Идентификатор устройства DeviceId.
     */
    DeviceId getId() const;

    /**
     * @brief Возвращает текущее имя устройства.
     * @return Копия строки с именем устройства.
     */
    std::string getName() const;
    /**
     * @brief Устанавливает новое имя устройства.
     *
     * Первый параметр (позиция 1) задает новое имя устройства std::string.
     */
    void setName(std::string);

  private:
    /**
     * @brief Имя устройства.
     */
    std::string name;
    /**
     * @brief Уникальный идентификатор устройства.
     */
    DeviceId id;
};

} // namespace smart_home
