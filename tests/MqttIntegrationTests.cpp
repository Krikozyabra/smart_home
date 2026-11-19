#include "mqtt/MosquittoClient.h"
#include "DeviceManagementTestSupport.h"
#include "TestUtil.h"
#include "device_management/DeviceManager.h"
#include "drivers/Zigbee2Mqtt.h"
#include "execution/CommandDispatcher.h"
#include "storage/InMemoryDeviceStorage.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern char** environ;
using namespace smart_home::mqtt;
using namespace std::chrono_literals;

namespace {
template<class F> void eventually(F&& predicate, const char* message, std::chrono::seconds timeout = 8s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return;
        std::this_thread::sleep_for(20ms);
    }
    throw std::runtime_error(message);
}

class LocalBroker {
public:
    explicit LocalBroker(std::string executable, bool anonymous = true) : executable_(std::move(executable)) {
        const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) throw std::runtime_error(std::string("Cannot create loopback socket: ") + std::strerror(errno));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            ::close(sock);
            throw std::runtime_error("Cannot reserve a temporary loopback port.");
        }
        socklen_t length = sizeof(address);
        if (::getsockname(sock, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
            ::close(sock);
            throw std::runtime_error("Cannot read temporary broker port.");
        }
        port_ = ntohs(address.sin_port);
        ::close(sock);
        char path[] = "/tmp/smart-home-mqtt-XXXXXX";
        const char* directory = ::mkdtemp(path);
        if (!directory) throw std::runtime_error("Cannot create broker temporary directory.");
        directory_ = directory;
        config_ = directory_ + "/mosquitto.conf";
        log_ = directory_ + "/broker.log";
        std::ofstream config(config_);
        config << "listener " << port_ << " 127.0.0.1\n"
               << "allow_anonymous " << (anonymous ? "true" : "false") << "\n"
               << "persistence false\n";
        config.close();
        try { start(); }
        catch (...) { stop(); std::filesystem::remove_all(directory_); throw; }
    }
    ~LocalBroker() {
        stop();
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }
    void start() {
        require(pid_ == -1, "Broker already running.");
        posix_spawn_file_actions_t files;
        require(posix_spawn_file_actions_init(&files) == 0, "Cannot prepare broker process.");
        const int opened = posix_spawn_file_actions_addopen(&files, STDOUT_FILENO, log_.c_str(),
                                                           O_WRONLY | O_CREAT | O_TRUNC, 0600);
        const int duplicated = posix_spawn_file_actions_adddup2(&files, STDOUT_FILENO, STDERR_FILENO);
        if (opened || duplicated) {
            posix_spawn_file_actions_destroy(&files);
            throw std::runtime_error("Cannot prepare broker log output.");
        }
        char* args[] = {executable_.data(), const_cast<char*>("-c"), config_.data(), nullptr};
        const int rc = posix_spawn(&pid_, executable_.c_str(), &files, nullptr, args, environ);
        posix_spawn_file_actions_destroy(&files);
        if (rc != 0) { pid_ = -1; throw std::runtime_error("Cannot launch test broker."); }
        eventually([&] {
            int status;
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                pid_ = -1;
                throw std::runtime_error("Temporary MQTT broker exited before startup; check socket permissions.");
            }
            const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return false;
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(static_cast<std::uint16_t>(port_));
            const bool ready = ::connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
            ::close(sock);
            return ready;
        }, "Temporary MQTT broker did not start.", 5s);
    }
    void stop() noexcept {
        if (pid_ == -1) return;
        ::kill(pid_, SIGTERM);
        for (int i = 0; i < 50; ++i) {
            int status;
            const auto result = waitpid(pid_, &status, WNOHANG);
            if (result == pid_ || (result == -1 && errno != EINTR)) { pid_ = -1; return; }
            std::this_thread::sleep_for(10ms);
        }
        ::kill(pid_, SIGKILL);
        int status;
        while (waitpid(pid_, &status, 0) == -1 && errno == EINTR) {}
        pid_ = -1;
    }
    MqttConfig config() const {
        MqttConfig config;
        config.host = "127.0.0.1";
        config.port = port_;
        config.connectTimeout = 2s;
        config.reconnectDelayInitial = 1;
        config.reconnectDelayMax = 1;
        return config;
    }
    int port() const noexcept { return port_; }
private:
    std::string executable_, directory_, config_, log_;
    int port_ = 0;
    pid_t pid_ = -1;
};

void confirmed(IMqttClient& client, MqttDeliveryToken token) {
    require(client.waitForOperation(token, 3s).status == MqttOperationStatus::Completed,
            "Broker confirmation missing.");
}

void zigbeeRoundTrip(LocalBroker &broker) {
    using namespace smart_home;
    const std::string topic = "zigbee2mqtt/living_room/lamp";
    MosquittoClient client;
    Zigbee2Mqtt transport(client);
    DriverFactoryRegistry creators;
    creators.add("zigbee", transport.creator());
    device_management_test::GraphFixture fixture;
    InMemoryDeviceStorage storage;
    const auto record = storage.insert("living_room/lamp", "zigbee", "Mixed device");
    const auto id = record.local_id;
    fixture.graph.addDevice({id, record.name});
    for (const auto *capability : {"on_off", "brightness", "color", "temperature",
                                   "humidity", "contact", "battery", "power"})
        fixture.graph.addDeviceAbility(id, capability);

    DeviceRegistry registry;
    DeviceManager manager(storage, creators, registry, fixture.graph);
    CommandDispatcher dispatcher(registry);
    require(manager.restoreStoredDevices() == 1 && !client.isConnected(),
            "Zigbee must restore capabilities without contacting the broker");
    require(dispatcher.execute({id, "brightness.get", {}}) == Value{50},
            "A cold Zigbee device must provide its initial state");
    expectException<MqttException>([&] { dispatcher.execute({id, "on_off.set", {true}}); });

    client.connect(broker.config());
    confirmed(client, client.subscribe(topic));
    MosquittoClient bridge;
    bridge.connect(broker.config());
    confirmed(bridge, bridge.subscribe(topic + "/set", MqttQoS::AtLeastOnce));

    const auto command = [&](const char *operation, Value value, const std::string &expected) {
        require(!dispatcher.execute({id, operation, {value}}), "A write must return no value");
        MqttMessage message;
        require(bridge.readFor(3s, message) && message.topic == topic + "/set" &&
                    std::string(message.payload.begin(), message.payload.end()) == expected &&
                    !message.retain,
                "A write must publish the expected Zigbee2MQTT command");
    };
    command("on_off.set", true, R"({"state":"ON"})");
    command("brightness.set", 75, R"({"brightness":191})");
    command("color.set", Color{12, 34, 56}, R"({"color":{"r":12,"g":34,"b":56}})");
    require(dispatcher.execute({id, "on_off.get", {}}) == Value{false} &&
                dispatcher.execute({id, "brightness.get", {}}) == Value{50} &&
                dispatcher.execute({id, "color.get", {}}) == Value{Color{255, 255, 255}},
            "Command publication must leave reported state until telemetry is received");

    auto &driver = *registry.find(id);
    const auto telemetry = [&](const std::string &payload) {
        confirmed(bridge, bridge.publish(topic, {payload.begin(), payload.end()},
                                         MqttQoS::AtLeastOnce));
        MqttMessage message;
        require(client.readFor(3s, message), "Zigbee telemetry did not arrive");
        return transport.applyTelemetry(driver, message);
    };
    require(telemetry(R"({"state":"ON","brightness":191,"color":{"r":12,"g":34,"b":56},"temperature":27.5,"humidity":44.5,"contact":false,"battery":80,"power":2.75})"),
            "Valid telemetry must update the device");
    require(dispatcher.execute({id, "on_off.get", {}}) == Value{true} &&
                dispatcher.execute({id, "brightness.get", {}}) == Value{75} &&
                dispatcher.execute({id, "color.get", {}}) == Value{Color{12, 34, 56}} &&
                dispatcher.execute({id, "temperature.get", {}}) == Value{27.5} &&
                dispatcher.execute({id, "humidity.get", {}}) == Value{44.5} &&
                dispatcher.execute({id, "contact.get", {}}) == Value{true} &&
                dispatcher.execute({id, "battery.get", {}}) == Value{80} &&
                dispatcher.execute({id, "power.get", {}}) == Value{2.75},
            "Reads must return the decoded values of all eight capabilities");
    require(!telemetry(R"({"state":"OFF"} trailing)") &&
                telemetry(R"({"temperature":210.0,"battery":"99","contact":true})") &&
                dispatcher.execute({id, "on_off.get", {}}) == Value{true} &&
                dispatcher.execute({id, "temperature.get", {}}) == Value{27.5} &&
                dispatcher.execute({id, "battery.get", {}}) == Value{80} &&
                dispatcher.execute({id, "contact.get", {}}) == Value{false},
            "Invalid data must preserve previous values and valid fields must apply");
    require(telemetry(R"({"color_mode":"hs","color":{"x":0.3127,"y":0.329,"hue":120,"saturation":100}})") &&
                dispatcher.execute({id, "color.get", {}}) == Value{Color{0, 255, 0}},
            "Hue and saturation telemetry must convert to RGB");
    require(telemetry(R"({"color":{"x":0.3127,"y":0.329}})"),
            "CIE xy telemetry must convert to RGB");
    require(!transport.applyTelemetry(driver, {topic + "/set", {'{', '}'}}),
            "A command topic must not change reported state");
}

void roundTrips(LocalBroker& broker) {
    MosquittoClient client;
    auto config = broker.config();
    client.connect(config);
    expectException<std::logic_error>([&] { client.connect(config); });
    const auto sub = client.subscribe("test/binary/#", MqttQoS::ExactlyOnce);
    const auto result = client.waitForOperation(sub, 3s);
    require(result.status == MqttOperationStatus::Completed && result.grantedQoS == 2,
            "SUBACK must report granted QoS.");
    const std::vector<std::uint8_t> binary{0, 1, 255, 0, 42};
    for (auto qos : {MqttQoS::AtMostOnce, MqttQoS::AtLeastOnce, MqttQoS::ExactlyOnce}) {
        confirmed(client, client.publish("test/binary/value", binary, qos));
        MqttMessage message;
        require(client.readFor(3s, message) && message.topic == "test/binary/value" && message.payload == binary,
                "Binary round-trip corrupted payload.");
    }
    confirmed(client, client.publish("test/binary/empty", {}, MqttQoS::AtLeastOnce));
    MqttMessage message;
    require(client.readFor(3s, message) && message.payload.empty(), "Zero-length payload must be supported.");
    confirmed(client, client.unsubscribe("test/binary/#"));
    confirmed(client, client.publish("test/binary/value", binary, MqttQoS::AtLeastOnce));
    require(!client.readFor(150ms, message), "Unsubscribe must stop message delivery.");
    client.disconnect();
    client.disconnect();
    require(client.state() == MqttConnectionState::Disconnected, "Disconnect must finish before returning.");
    client.connect(config);
    confirmed(client, client.subscribe("test/restarted"));
    confirmed(client, client.publish("test/restarted", binary));
    require(client.readFor(3s, message), "Explicit reconnect must produce a usable client.");
}

void retained(LocalBroker& broker) {
    MosquittoClient publisher;
    publisher.connect(broker.config());
    const std::vector<std::uint8_t> binary{0, 255, 2};
    confirmed(publisher, publisher.publish("test/retained", binary, MqttQoS::AtLeastOnce, true));
    MosquittoClient subscriber;
    subscriber.connect(broker.config());
    confirmed(subscriber, subscriber.subscribe("test/retained", MqttQoS::AtLeastOnce));
    MqttMessage message;
    require(subscriber.readFor(3s, message) && message.payload == binary && message.retain,
            "Retained subscription must preserve binary data and retain flag.");
    confirmed(publisher, publisher.publish("test/retained", {}, MqttQoS::AtLeastOnce, true));
}

void limitsAndConcurrency(LocalBroker& broker) {
    auto config = broker.config();
    config.receiveQueueCapacity = 1;
    config.receiveQueueBytes = 64;
    MosquittoClient reader;
    reader.connect(config);
    confirmed(reader, reader.subscribe("test/overflow", MqttQoS::AtLeastOnce));
    MosquittoClient writer;
    writer.connect(broker.config());
    for (std::uint8_t i = 0; i < 6; ++i)
        confirmed(writer, writer.publish("test/overflow", {i}, MqttQoS::AtLeastOnce));
    eventually([&] { return reader.droppedMessages() == 5; }, "Receive queue overflow was not observable.");
    MqttMessage first;
    require(reader.tryRead(first) && first.payload == std::vector<std::uint8_t>({0}),
            "Drop-incoming policy must preserve queued messages.");

    std::vector<std::future<void>> producers;
    for (int i = 0; i < 4; ++i) producers.push_back(std::async(std::launch::async, [&] {
        for (int j = 0; j < 50; ++j)
            require(writer.waitForDelivery(writer.publish("test/concurrent", {0, 255}), 3s),
                    "Concurrent QoS 0 ACK registration lost a completion.");
    }));
    for (auto& producer : producers) producer.get();

    config = broker.config();
    config.maxPendingOperations = 1;
    config.maxPendingPublishBytes = 4;
    config.maxPayloadBytes = 4;
    MosquittoClient bounded;
    bounded.connect(config);
    const auto token = bounded.publish("test/limits", {0, 1, 2, 3}, MqttQoS::AtLeastOnce);
    expectException<MqttException>([&] { bounded.publish("test/limits", {}); });
    confirmed(bounded, token);
    expectException<std::invalid_argument>([&] { bounded.publish("test/limits", {0, 1, 2, 3, 4}); });
    confirmed(bounded, bounded.publish("test/limits", {}));

    auto waiting = std::async(std::launch::async, [&] {
        MqttMessage message;
        return bounded.readFor(10s, message);
    });
    bounded.disconnect();
    require(waiting.wait_for(1s) == std::future_status::ready && !waiting.get(),
            "Client shutdown must wake application readers.");
}

void reconnection(LocalBroker& broker) {
    auto config = broker.config();
    config.cleanSession = false;
    config.clientId = "reconnect-test";
    MosquittoClient reader;
    reader.connect(config);
    confirmed(reader, reader.subscribe("test/reconnect", MqttQoS::AtLeastOnce));
    confirmed(reader, reader.subscribe("test/removed", MqttQoS::AtLeastOnce));
    confirmed(reader, reader.unsubscribe("test/removed"));
    broker.stop();
    eventually([&] { return reader.state() == MqttConnectionState::Connecting; }, "Connection loss was not reported.");
    expectException<MqttException>([&] { reader.publish("test/reconnect", {1}); });
    broker.start();
    eventually([&] { return reader.isConnected(); }, "Automatic MQTT reconnect failed.");
    confirmed(reader, reader.publish("test/reconnect", {7, 0, 255}, MqttQoS::AtLeastOnce));
    MqttMessage message;
    require(reader.readFor(3s, message) && message.payload == std::vector<std::uint8_t>({7, 0, 255}),
            "Reconnect did not restore the subscription.");
    confirmed(reader, reader.publish("test/removed", {1}, MqttQoS::AtLeastOnce));
    require(!reader.readFor(150ms, message), "Reconnect resurrected an unsubscribed filter.");
    broker.stop();
    eventually([&] { return !reader.isConnected(); }, "Second disconnect was not detected.");
    const auto start = std::chrono::steady_clock::now();
    reader.disconnect();
    require(std::chrono::steady_clock::now() - start < 3s, "Shutdown while reconnecting took too long.");
    broker.start();
}

void refused(const std::string& executable) {
    LocalBroker restricted(executable, false);
    MosquittoClient client;
    bool rejected = false;
    try { client.connect(restricted.config()); }
    catch (const MqttConnectionException& error) {
        rejected = true;
        require(error.errorCode() == 5 || error.errorCode() == 4, "Unexpected CONNACK rejection code.");
    }
    require(rejected && client.state() == MqttConnectionState::ConnectionFailed,
            "Anonymous rejection must be exposed, with automatic retry stopped.");
    client.disconnect();
}

void demonstration(std::string executable, int port) {
    std::string number = std::to_string(port);
    char* args[] = {executable.data(), const_cast<char*>("--host"), const_cast<char*>("127.0.0.1"),
        const_cast<char*>("--port"), number.data(), const_cast<char*>("--topic"),
        const_cast<char*>("test/demo"), const_cast<char*>("--timeout"), const_cast<char*>("2"), nullptr};
    posix_spawn_file_actions_t files;
    require(posix_spawn_file_actions_init(&files) == 0, "Cannot prepare demo process.");
    posix_spawn_file_actions_addopen(&files, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_adddup2(&files, STDOUT_FILENO, STDERR_FILENO);
    pid_t pid;
    const int rc = posix_spawn(&pid, executable.c_str(), &files, nullptr, args, environ);
    posix_spawn_file_actions_destroy(&files);
    require(rc == 0, "Cannot launch MQTT demo.");
    for (int i = 0; i < 500; ++i) {
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "MQTT demo round-trip failed.");
            return;
        }
        std::this_thread::sleep_for(20ms);
    }
    ::kill(pid, SIGKILL);
    int status;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
    throw std::runtime_error("MQTT demo did not terminate within its test deadline.");
}
}

int main(int argc, char* argv[]) {
    try {
        require(argc == 2 || argc == 3, "Pass the Mosquitto broker executable and optionally the demo.");
        LocalBroker broker(argv[1]);
        roundTrips(broker);
        zigbeeRoundTrip(broker);
        retained(broker);
        limitsAndConcurrency(broker);
        reconnection(broker);
        refused(argv[1]);
        if (argc == 3) demonstration(argv[2], broker.port());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
