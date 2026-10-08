/** @file MosquittoClient.h
 * @brief Клиент MQTT 3.1.1 на основе libmosquittopp со скрытым транспортом.
 */
#pragma once

#include "mqtt/interfaces/IMqttClient.h"
#include <memory>

namespace smart_home::mqtt {

/** @brief Реализация IMqttClient; сетевые callback не выполняют пользовательский код. */
class MosquittoClient final : public IMqttClient {
public:
    /** @brief Создаёт отключённый клиент.
     * @throws MqttException Не удалось инициализировать библиотеку.
     */
    MosquittoClient();
    /** @brief Останавливает поток до освобождения транспорта и библиотеки. */
    ~MosquittoClient() override;
    MosquittoClient(const MosquittoClient&) = delete; ///< Копирование запрещено.
    MosquittoClient& operator=(const MosquittoClient&) = delete; ///< Копирование запрещено.
    /** @brief Перемещает клиент вместе с работающим транспортом.
     * @param other Исходный клиент; одновременный доступ к нему запрещён.
     */
    MosquittoClient(MosquittoClient&& other) noexcept;
    /** @brief Останавливает текущий клиент и перемещает другой.
     * @param other Исходный клиент; одновременный доступ к нему запрещён.
     * @return Ссылка на текущий объект.
     */
    MosquittoClient& operator=(MosquittoClient&& other) noexcept;

    /// @copydoc IMqttClient::connect
    void connect(const MqttConfig& config) override;
    /// @copydoc IMqttClient::disconnect
    void disconnect() noexcept override;
    /// @copydoc IMqttClient::isConnected
    bool isConnected() const noexcept override;
    /// @copydoc IMqttClient::state
    MqttConnectionState state() const noexcept override;
    /// @copydoc IMqttClient::publish
    MqttDeliveryToken publish(std::string_view topic, const std::vector<std::uint8_t>& payload,
        MqttQoS qos = MqttQoS::AtMostOnce, bool retain = false) override;
    /// @copydoc IMqttClient::subscribe
    MqttDeliveryToken subscribe(std::string_view topicFilter,
        MqttQoS qos = MqttQoS::AtMostOnce) override;
    /// @copydoc IMqttClient::unsubscribe
    MqttDeliveryToken unsubscribe(std::string_view topicFilter) override;
    /// @copydoc IMqttClient::tryRead
    bool tryRead(MqttMessage& outMessage) override;
    /// @copydoc IMqttClient::readFor
    bool readFor(std::chrono::milliseconds timeout, MqttMessage& outMessage) override;
    /// @copydoc IMqttClient::waitForOperation
    MqttOperationResult waitForOperation(MqttDeliveryToken token,
        std::chrono::milliseconds timeout) override;
    /// @copydoc IMqttClient::waitForDelivery
    bool waitForDelivery(MqttDeliveryToken token, std::chrono::milliseconds timeout) override;
    /// @copydoc IMqttClient::droppedMessages
    std::uint64_t droppedMessages() const noexcept override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::mqtt
