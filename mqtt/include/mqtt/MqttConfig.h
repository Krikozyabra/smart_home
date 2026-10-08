/** @file MqttConfig.h
 * @brief Настройки подключения к произвольному брокеру MQTT 3.1.1.
 */
#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace smart_home::mqtt {

/** @brief Сертификаты TLS; закрытый ключ должен быть без парольного шифрования. */
struct MqttTlsConfig {
    std::string caFile; ///< Читаемый файл доверенных сертификатов CA.
    std::string clientCertFile; ///< Необязательный сертификат клиента.
    std::string clientKeyFile; ///< Необязательный ключ; задаётся вместе с сертификатом.
    bool verifyPeer = true; ///< Проверять сертификат и имя брокера.
};

/** @brief Конфигурация клиента; адрес и темы задаёт вызывающий код. */
struct MqttConfig {
    std::string host; ///< Обязательный адрес брокера.
    int port = 1883; ///< Порт 1–65535; для TLS явно задайте, например, 8883.
    std::string clientId; ///< Пустой означает генерацию библиотекой при cleanSession=true.
    bool cleanSession = true; ///< MQTT Clean Session; false требует непустой clientId.
    int keepaliveSeconds = 60; ///< Keepalive от 5 до 65535 секунд.
    std::optional<std::string> username; ///< Необязательное имя пользователя.
    std::optional<std::string> password; ///< Необязательный пароль; требует username.
    std::optional<MqttTlsConfig> tls; ///< nullopt означает обычный TCP.
    std::chrono::milliseconds connectTimeout{5000}; ///< Ожидание CONNACK после запуска цикла.
    unsigned int reconnectDelayInitial = 1; ///< Начальная задержка повторной попытки, секунд.
    unsigned int reconnectDelayMax = 30; ///< Максимальная задержка с экспоненциальным ростом.
    std::size_t receiveQueueCapacity = 1000; ///< Максимум входящих сообщений.
    std::size_t receiveQueueBytes = 16 * 1024 * 1024; ///< Лимит суммарных байтов темы и данных.
    std::size_t maxPayloadBytes = 16 * 1024 * 1024; ///< Максимальная нагрузка одного сообщения.
    std::size_t maxPendingOperations = 256; ///< Лимит токенов, включая непрочитанные результаты.
    std::size_t maxPendingPublishBytes = 16 * 1024 * 1024; ///< Лимит данных до подтверждения.
    std::size_t maxSubscriptions = 128; ///< Лимит фильтров, включая ожидающие отписки.

    /** @brief Проверяет настройки до открытия соединения.
     * @throws std::invalid_argument Некорректное значение либо недоступный файл TLS.
     * @throws MqttException Не удалось инициализировать библиотеку.
     */
    void validate() const;
};

} // namespace smart_home::mqtt
