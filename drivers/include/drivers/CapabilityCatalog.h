/**
 * @file CapabilityCatalog.h
 * @brief Связи стандартных возможностей со свойствами устройств.
 */

#pragma once

#include "descriptors/CapabilityDescriptor.h"
#include "devices/PropertyTag.h"

#include <optional>
#include <string_view>
#include <variant>

namespace smart_home {

class Device;

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
        T initial_value;                 ///< Значение до первого обновления.
    };

    /** @brief Запись каталога для выбора возможности по строковому ID. */
    struct Definition {
        const CapabilityDescriptor
            &descriptor; ///< Дескриптор со сроком жизни каталога.
        std::variant<PropertyTag<bool>, PropertyTag<int>, PropertyTag<double>,
                     PropertyTag<Color>>
            property;        ///< Типизированная метка.
        Value initial_value; ///< Начальное значение.

        /// @brief Возвращает ключ в словаре Device.
        const char *key() const noexcept;
        /// @brief Проверяет наличие операции записи.
        bool writable() const noexcept;
        /// @brief Проверяет тип и диапазон значения.
        bool accepts(const Value &value) const noexcept;
        /// @brief Возвращает копию свойства ожидаемого типа, если оно есть.
        std::optional<Value> read(const Device &device) const;
        /**
         * @brief Записывает свойство через его метку.
         * @throws std::invalid_argument При неверном типе или диапазоне.
         */
        void store(Device &device, const Value &value) const;
    };

    /**
     * @brief Возвращает запись каталога или nullptr для неизвестного ID.
     * @return Указатель действителен до завершения программы.
     */
    static const Definition *find(std::string_view capability_id);

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
