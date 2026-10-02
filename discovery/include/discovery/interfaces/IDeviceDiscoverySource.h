/**
 * @file IDeviceDiscoverySource.h
 * @brief Интерфейс для источников обнаружения устройств умного дома.
 */

#pragma once

#include "discovery/entities/DiscoveredDevice.h"

#include <string_view>
#include <vector>

namespace smart_home {

/**
 * @class IDeviceDiscoverySource
 * @brief Абстрактный интерфейс источника обнаружения устройств.
 */
class IDeviceDiscoverySource {
  public:
    /**
     * @brief Виртуальный деструктор по умолчанию.
     */
    virtual ~IDeviceDiscoverySource() = default;

    /**
     * @brief Выполняет сканирование для поиска доступных устройств.
     * @return Вектор обнаруженных устройств DiscoveredDevice.
     */
    virtual std::vector<DiscoveredDevice> scan() = 0;

    /**
     * @brief Возвращает имя класса источника обнаружения.
     * @return Имя класса источника обнаружения в виде string_view.
     */
    virtual std::string_view get_class_name() const = 0;
};

} // namespace smart_home
