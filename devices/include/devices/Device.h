/**
 * @file Device.h
 * @brief Цифровой двойник устройства умного дома.
 */

#pragma once

#include "common/DeviceId.h"
#include "devices/PropertyTag.h"
#include "devices/types/Value.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace smart_home {

/**
 * @brief Хранит локальный идентификатор, имя и свойства устройства в памяти.
 * @details Устройство владеет ключами и значениями свойств. Вызывающий код
 * синхронизирует доступ к устройству и всем его представлениям.
 */
class Device final {
  public:
    /**
     * @brief Создаёт устройство с пустым словарём свойств.
     * @param id Локальный идентификатор устройства.
     * @param name Начальное имя устройства.
     */
    Device(DeviceId id, std::string name);

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) noexcept = default;
    Device &operator=(Device &&) noexcept = default;

    /// @brief Возвращает локальный идентификатор устройства.
    DeviceId getId() const noexcept;

    /// @brief Возвращает копию текущего имени устройства.
    std::string getName() const;

    /**
     * @brief Устанавливает новое имя устройства.
     * @param name Новое имя.
     * @throws std::invalid_argument Если имя пустое.
     */
    void setName(std::string name);

    /**
     * @brief Добавляет свойство или заменяет его значение и тип.
     * @param key Ключ, копируемый при вставке.
     * @param value Новое значение. Границы меток проверяет метод set().
     * @throws std::invalid_argument Если ключ пустой.
     */
    void setProperty(const std::string &key, Value value);

    /**
     * @brief Возвращает копию значения или std::nullopt при отсутствии ключа.
     * @throws std::invalid_argument Если ключ пустой.
     */
    std::optional<Value> getProperty(const std::string &key) const;

    /**
     * @brief Проверяет наличие ключа свойства.
     * @throws std::invalid_argument Если ключ пустой.
     */
    bool hasProperty(const std::string &key) const;

    /**
     * @brief Удаляет свойство по ключу.
     * @return true, если свойство существовало.
     * @throws std::invalid_argument Если ключ пустой.
     */
    bool removeProperty(const std::string &key);

    /**
     * @brief Возвращает доступный только для чтения словарь текущих свойств.
     * @return Ссылка на словарь действительна в течение жизни устройства.
     */
    const std::unordered_map<std::string, Value> &
    getProperties() const noexcept;

    /**
     * @brief Записывает значение свойства после проверки границ метки.
     * @param tag Типизированная метка свойства.
     * @param value Новое значение.
     * @throws std::invalid_argument Если ключ пустой или значение вне границ.
     * @details При ошибке проверки словарь сохраняет прежнее состояние.
     */
    template <typename T> void set(const PropertyTag<T> &tag, const T &value) {
        if (!tag.validate(value))
            throw std::invalid_argument("Property '" + std::string(tag.key) +
                                        "' is out of range");
        setProperty(tag.key, Value{value});
    }

    /**
     * @brief Возвращает копию значения указанного в метке типа.
     * @return std::nullopt, если ключ отсутствует или тип значения отличается.
     * @throws std::invalid_argument Если ключ метки пустой.
     */
    template <typename T>
    std::optional<T> get(const PropertyTag<T> &tag) const {
        if (const auto *value = std::get_if<T>(findProperty(tag.key)))
            return *value;
        return std::nullopt;
    }

    /**
     * @brief Проверяет наличие ключа метки независимо от типа значения.
     * @throws std::invalid_argument Если ключ метки пустой.
     */
    template <typename T> bool has(const PropertyTag<T> &tag) const {
        return hasProperty(tag.key);
    }

    /**
     * @brief Создаёт невладеющее представление устройства.
     * @tparam ViewType Представление с конструктором от Device&.
     * @return Представление, привязанное к этому объекту по его адресу.
     * Устройство должно существовать при каждом обращении через представление.
     */
    template <typename ViewType> ViewType view() & { return ViewType(*this); }

    /**
     * @brief Создаёт невладеющее представление константного устройства.
     * @tparam ViewType Представление с конструктором от const Device&.
     * @return Представление действует в течение жизни этого устройства.
     */
    template <typename ViewType> ViewType view() const & {
        return ViewType(*this);
    }

    template <typename ViewType> ViewType view() const && = delete;

  private:
    const Value *findProperty(const std::string &key) const;

    DeviceId id_;
    std::string name_;
    std::unordered_map<std::string, Value> properties_;
};

} // namespace smart_home
