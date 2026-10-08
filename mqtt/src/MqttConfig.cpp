#include "mqtt/MqttConfig.h"
#include "MosquittoApi.h"
#include "MosquittoGlobal.h"

#include <fstream>
#include <stdexcept>

namespace smart_home::mqtt {
namespace {
void require(bool valid, const char* message) {
    if (!valid) throw std::invalid_argument(message);
}
void validateText(const std::string& value, const char* message) {
    require(value.size() <= 65535 && value.find('\0') == std::string::npos, message);
    require(mosquitto_validate_utf8(value.data(), static_cast<int>(value.size())) == MOSQ_ERR_SUCCESS,
            message);
}
void readable(const std::string& file) {
    require(!file.empty() && file.find('\0') == std::string::npos &&
            static_cast<bool>(std::ifstream(file, std::ios::binary)),
            "MQTT TLS certificate or key file is not readable.");
}
}

void MqttConfig::validate() const {
    detail::MosquittoGlobal runtime;
    require(!host.empty() && host.find('\0') == std::string::npos, "MQTT host is required.");
    require(port >= 1 && port <= 65535, "MQTT port must be between 1 and 65535.");
    require(keepaliveSeconds >= 5 && keepaliveSeconds <= 65535, "Invalid MQTT keepalive.");
    validateText(clientId, "Invalid MQTT client ID.");
    require(cleanSession || !clientId.empty(), "Persistent MQTT sessions require a client ID.");
    require(!password || username.has_value(), "MQTT password requires a username.");
    if (username) validateText(*username, "Invalid MQTT username.");
    // The C++ wrapper accepts the password as a C string.
    if (password) require(password->size() <= 65535 && password->find('\0') == std::string::npos,
                          "Invalid MQTT password length or embedded NUL.");
    require(connectTimeout.count() > 0 && connectTimeout <= std::chrono::hours(24),
            "Invalid MQTT connection timeout.");
    require(reconnectDelayInitial > 0 && reconnectDelayMax >= reconnectDelayInitial &&
            reconnectDelayMax <= 86400, "Invalid MQTT reconnect delays.");
    require(receiveQueueCapacity > 0 && receiveQueueCapacity <= 1000000,
            "Invalid MQTT receive queue capacity.");
    constexpr std::size_t packetLimit = 268435455;
    require(maxPayloadBytes > 0 && maxPayloadBytes <= packetLimit, "Invalid MQTT payload limit.");
    require(receiveQueueBytes > 0 && receiveQueueBytes <= packetLimit,
            "Invalid MQTT receive byte limit.");
    require(maxPendingOperations > 0 && maxPendingOperations <= 4096,
            "Invalid MQTT pending operation limit.");
    require(maxPendingPublishBytes > 0 && maxPendingPublishBytes <= packetLimit,
            "Invalid MQTT pending publish byte limit.");
    require(maxSubscriptions > 0 && maxSubscriptions <= 4096, "Invalid MQTT subscription limit.");
    if (tls) {
        readable(tls->caFile);
        require(tls->clientCertFile.empty() == tls->clientKeyFile.empty(),
                "MQTT client certificate and key must be configured together.");
        if (!tls->clientCertFile.empty()) {
            readable(tls->clientCertFile);
            readable(tls->clientKeyFile);
        }
    }
}

} // namespace smart_home::mqtt
