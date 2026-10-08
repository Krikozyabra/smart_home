/** @file MqttErrors.h
 * @brief Исключения транспорта MQTT без секретных данных в диагностике.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace smart_home::mqtt {

/** @brief Ошибка локальной библиотеки или лимита ресурсов клиента. */
class MqttException : public std::runtime_error {
public:
    /** @brief Создаёт исключение.
     * @param message Безопасное описание без адресов, паролей и содержимого сообщений.
     * @param code Код библиотеки; 0 для собственных ошибок клиента.
     */
    explicit MqttException(const std::string& message, int code = 0)
        : std::runtime_error(message), code_(code) {}
    /** @brief Возвращает код ошибки. @return Код библиотеки либо 0. */
    int errorCode() const noexcept { return code_; }
private:
    int code_;
};

/** @brief Отказ MQTT 3.1.1 CONNACK; пространство кодов отличается от ошибок библиотеки. */
class MqttConnectionException : public MqttException {
public:
    /** @brief Создаёт исключение отказа брокера.
     * @param code Код CONNACK (1–5).
     */
    explicit MqttConnectionException(int code)
        : MqttException("MQTT broker rejected the connection.", code) {}
};

} // namespace smart_home::mqtt
