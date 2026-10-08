#include "mqtt/MosquittoClient.h"
#include "logging/Logging.h"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
int number(std::string_view text, int minimum, int maximum) {
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value < minimum || value > maximum)
        throw std::invalid_argument("Invalid numeric MQTT demo option.");
    return value;
}
void usage() {
    std::cout << "Usage: smart_home_mqtt_demo --host HOST --topic TOPIC "
                 "[--port PORT] [--timeout SECONDS] [--ca-file FILE]\n"
                 "Credentials: SMART_HOME_MQTT_USERNAME and SMART_HOME_MQTT_PASSWORD.\n";
}
}

int main(int argc, char* argv[]) {
    namespace mqtt = smart_home::mqtt;
    namespace logging = smart_home::logging;
    mqtt::MqttConfig config;
    std::string topic;
    int seconds = 5;
    bool explicitPort = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string_view option(argv[i]);
            if (option == "--help") { usage(); return 0; }
            if (i + 1 == argc) throw std::invalid_argument("Missing MQTT demo option value.");
            const std::string value(argv[++i]);
            if (option == "--host") config.host = value;
            else if (option == "--topic") topic = value;
            else if (option == "--port") { config.port = number(value, 1, 65535); explicitPort = true; }
            else if (option == "--timeout") seconds = number(value, 1, 60);
            else if (option == "--ca-file") {
                config.tls = mqtt::MqttTlsConfig{};
                config.tls->caFile = value;
            } else throw std::invalid_argument("Unknown MQTT demo option.");
        }
        if (config.host.empty() || topic.empty()) {
            usage();
            throw std::invalid_argument("MQTT demo requires --host and --topic.");
        }
        if (config.tls && !explicitPort) config.port = 8883;
        if (const char* value = std::getenv("SMART_HOME_MQTT_USERNAME")) config.username = value;
        if (const char* value = std::getenv("SMART_HOME_MQTT_PASSWORD")) config.password = value;
        config.connectTimeout = std::chrono::seconds(seconds);
        config.validate();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }

    logging::Config logs;
    logs.file_path.clear();
    try {
        logging::Session session(logs);
        mqtt::MosquittoClient client;
        client.connect(config);
        const auto timeout = std::chrono::seconds(seconds);
        const auto subscription = client.subscribe(topic, mqtt::MqttQoS::AtLeastOnce);
        if (client.waitForOperation(subscription, timeout).status != mqtt::MqttOperationStatus::Completed)
            throw mqtt::MqttException("MQTT subscription was rejected or timed out.");
        const std::vector<std::uint8_t> payload{'M', 'Q', 'T', 'T', 0, 'd', 'e', 'm', 'o'};
        const auto publication = client.publish(topic, payload, mqtt::MqttQoS::AtLeastOnce);
        if (!client.waitForDelivery(publication, timeout))
            throw mqtt::MqttException("MQTT publish confirmation timed out.");
        mqtt::MqttMessage received;
        if (!client.readFor(timeout, received))
            throw mqtt::MqttException("MQTT message read timed out.");
        std::cout << "Received " << received.payload.size() << " bytes on " << received.topic << '\n';
        client.disconnect();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MQTT demo failed: " << error.what() << '\n';
        return 1;
    }
}
