#pragma once

#include "drivers/DeviceCreator.h"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace smart_home {

/**
 * @brief Реестр создателей драйверов по идентификатору протокола.
 * @details Владеет функциями создания. Вызывающий код сериализует доступ.
 */
class DriverFactoryRegistry {
  public:
    /**
     * @brief Регистрирует создателя протокола.
     * @param driver_id Непустой идентификатор протокола.
     * @param creator Функция создания, передаваемая во владение реестра.
     * @throws std::invalid_argument Пустой идентификатор или функция.
     * @throws std::runtime_error Протокол уже зарегистрирован.
     */
    void add(const std::string &driver_id, DeviceCreator creator);

    /**
     * @brief Возвращает создателя протокола или nullptr.
     * @return Указатель действителен в течение жизни реестра.
     */
    const DeviceCreator *find(const std::string &driver_id) const;

    /// @brief Возвращает число зарегистрированных протоколов.
    std::size_t size() const noexcept;

  private:
    std::unordered_map<std::string, DeviceCreator> creators_;
};

} // namespace smart_home
