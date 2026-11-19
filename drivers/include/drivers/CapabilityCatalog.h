/**
 * @file CapabilityCatalog.h
 * @brief Связи стандартных возможностей со свойствами устройств.
 */

#pragma once

#include "descriptors/CapabilityDescriptor.h"
#include "devices/PropertyTag.h"

namespace smart_home {

/**
 * @brief Каталог стандартных возможностей устройств.
 * @details Записи инициализируются потокобезопасно при первом обращении.
 * Ссылки на записи действительны до завершения программы.
 */
class CapabilityCatalog {
  public:
    /**
     * @brief Описание операций возможности и типизированная метка её свойства.
     * @tparam T Тип значения свойства.
     */
    template <typename T> struct Entry {
        PropertyTag<T> property;         ///< Метка свойства устройства.
        CapabilityDescriptor descriptor; ///< ID возможности и её операции.
    };

    /// @brief Возвращает возможность включения и выключения.
    static const Entry<bool> &onOff();
    /// @brief Возвращает возможность чтения и записи яркости.
    static const Entry<int> &brightness();
    /// @brief Возвращает возможность чтения и записи цвета RGB.
    static const Entry<Color> &color();
    /// @brief Возвращает возможность чтения температуры.
    static const Entry<double> &temperature();
    /// @brief Возвращает возможность чтения влажности.
    static const Entry<double> &humidity();
    /// @brief Возвращает возможность чтения состояния контакта.
    static const Entry<bool> &contact();
    /// @brief Возвращает возможность чтения уровня заряда.
    static const Entry<int> &battery();
    /// @brief Возвращает возможность чтения активной мощности.
    static const Entry<double> &power();
};

} // namespace smart_home
