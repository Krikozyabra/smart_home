/** @file MqttTypes.h
 * @brief Типы сообщений, состояния подключения и результатов операций MQTT.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace smart_home::mqtt {

/** @brief Уровень доставки на участке протокола MQTT; не гарантия выполнения команды. */
enum class MqttQoS : std::uint8_t {
    AtMostOnce = 0, ///< QoS 0: без подтверждения брокера.
    AtLeastOnce = 1, ///< QoS 1: возможны повторные сообщения.
    ExactlyOnce = 2 ///< QoS 2: протокольный обмен с устранением повторов.
};

/** @brief Состояние клиента; Connecting также означает автоматическое переподключение. */
enum class MqttConnectionState {
    Disconnected, Connecting, Connected, Disconnecting, ConnectionFailed
};

/** @brief Собственный токен операции; не переиспользуемый 16-битный идентификатор MQTT. */
using MqttDeliveryToken = std::uint64_t;

/** @brief Результат операции публикации, подписки или отписки. */
enum class MqttOperationStatus {
    Pending, ///< Подтверждение ещё не получено, в том числе после тайм-аута ожидания.
    Completed, ///< Получено соответствующее протокольное подтверждение.
    Rejected, ///< Брокер отклонил подписку.
    Cancelled, ///< Операция отменена при остановке или потере соединения.
    Unknown ///< Токен отсутствует либо результат уже прочитан.
};

/** @brief Результат ожидания операции. */
struct MqttOperationResult {
    MqttOperationStatus status = MqttOperationStatus::Unknown; ///< Состояние операции.
    int grantedQoS = -1; ///< QoS из SUBACK; -1 для прочих операций.
};

/** @brief Сообщение с собственной копией темы и бинарной полезной нагрузки. */
struct MqttMessage {
    std::string topic; ///< Тема сообщения.
    std::vector<std::uint8_t> payload; ///< Байты, включая нулевые; может быть пустой.
    MqttQoS qos = MqttQoS::AtMostOnce; ///< QoS полученного сообщения.
    bool retain = false; ///< Флаг входящего сообщения; не копия флага исходной публикации.
};

} // namespace smart_home::mqtt
