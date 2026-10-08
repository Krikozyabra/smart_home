#include "mqtt/MosquittoClient.h"
#include "MessageQueue.h"
#include "OperationTracker.h"
#include "TestUtil.h"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <utility>

using namespace smart_home::mqtt;
using namespace std::chrono_literals;

namespace {
void configuration() {
    MqttConfig config;
    expectException<std::invalid_argument>([&] { config.validate(); });
    config.host = "127.0.0.1";
    config.validate();
    const auto invalid = [&](auto change) {
        auto bad = config;
        change(bad);
        expectException<std::invalid_argument>([&] { bad.validate(); });
    };
    invalid([](auto& c) { c.port = 0; });
    invalid([](auto& c) { c.port = 65536; });
    invalid([](auto& c) { c.host = std::string("host\0tail", 9); });
    invalid([](auto& c) { c.clientId = std::string(1, '\xff'); });
    invalid([](auto& c) { c.cleanSession = false; });
    invalid([](auto& c) { c.password = "secret"; });
    invalid([](auto& c) { c.keepaliveSeconds = 4; });
    invalid([](auto& c) { c.connectTimeout = 0ms; });
    invalid([](auto& c) { c.receiveQueueCapacity = 0; });
    invalid([](auto& c) { c.receiveQueueBytes = 0; });
    invalid([](auto& c) { c.maxPendingOperations = 0; });
    invalid([](auto& c) { c.maxPendingPublishBytes = 0; });
    invalid([](auto& c) { c.maxPayloadBytes = 268435456; });
    invalid([](auto& c) { c.reconnectDelayMax = 0; });
    invalid([](auto& c) { c.tls = MqttTlsConfig{}; });
    config.clientId = "unit-test";
    config.cleanSession = false;
    config.username = "user";
    config.password = "secret";
    config.validate();
}

void queue() {
    detail::MessageQueue queue;
    queue.reset(2, 6);
    MqttMessage original{"a", {0, 255}, MqttQoS::AtLeastOnce, true};
    require(queue.push(original), "First message should fit.");
    original.payload[0] = 42;
    require(queue.push({"b", {1, 2}, MqttQoS::AtMostOnce, false}), "Second message should fit.");
    require(!queue.push({"c", {}, MqttQoS::AtMostOnce, false}), "Capacity must reject incoming message.");
    require(queue.dropped() == 1, "Overflow must be observable.");
    MqttMessage message;
    require(queue.readFor(0ms, message) && message.topic == "a" &&
        message.payload == std::vector<std::uint8_t>({0, 255}) && message.retain,
        "Queue must own binary data and preserve metadata and FIFO.");
    require(queue.readFor(0ms, message) && message.topic == "b", "Second FIFO entry missing.");
    require(!queue.readFor(15ms, message) && message.topic == "b", "Timeout must preserve output.");
    require(!queue.push({"large", {0, 1}, MqttQoS::AtMostOnce, false}), "Byte limit must include topic.");
    require(queue.push({"empty", {}, MqttQoS::AtMostOnce, false}), "Empty binary payload should fit.");
    queue.stop();
    require(queue.readFor(0ms, message) && message.payload.empty(), "Stopped queue must remain drainable.");
    require(!queue.readFor(10s, message), "Stopped queue must return without waiting.");
    expectException<std::invalid_argument>([&] { queue.readFor(-1ms, message); });

    queue.reset(2, 20);
    auto waiting = std::async(std::launch::async, [&] {
        MqttMessage out;
        return queue.readFor(5s, out);
    });
    queue.stop();
    require(waiting.wait_for(1s) == std::future_status::ready && !waiting.get(),
            "Shutdown must wake a waiting reader.");
    queue.reset(2, 20);
    auto receiving = std::async(std::launch::async, [&] {
        MqttMessage out;
        return queue.readFor(2s, out) && out.payload == std::vector<std::uint8_t>({7, 0, 9});
    });
    queue.push({"wake", {7, 0, 9}, MqttQoS::AtMostOnce, false});
    require(receiving.get(), "Incoming message must wake reader.");
}

void confirmations() {
    using Kind = detail::OperationKind;
    using Status = MqttOperationStatus;
    detail::OperationTracker tracker;
    tracker.reset(2, 1, 4);
    const auto first = tracker.begin(Kind::Publish, 4);
    expectException<MqttException>([&] { tracker.begin(Kind::Publish, 1); });
    // Exercise ACK-before-return, including synchronous QoS 0 completion.
    tracker.acknowledge(Kind::Publish, 12, {Status::Completed, -1});
    tracker.registerMid(first, 12);
    require(tracker.take(first).status == Status::Completed, "Early ACK was lost.");
    require(tracker.take(first).status == Status::Unknown, "Result must be consumed once.");
    const auto sub = tracker.begin(Kind::Subscribe);
    tracker.registerMid(sub, 13);
    require(tracker.take(sub).status == Status::Pending, "Timeout must not remove a pending token.");
    tracker.acknowledge(Kind::Publish, 13, {Status::Completed, -1});
    require(tracker.peek(sub).status == Status::Pending, "ACK kind must match request kind.");
    tracker.acknowledge(Kind::Subscribe, 13, {Status::Rejected, -1});
    require(tracker.take(sub).status == Status::Rejected, "SUBACK rejection must be preserved.");
    const auto reused = tracker.begin(Kind::Subscribe);
    tracker.registerMid(reused, 13);
    require(reused != sub && tracker.peek(reused).status == Status::Pending,
            "Reused wire MID must not reuse an old completion.");
    tracker.cancel(false);
    require(tracker.take(reused).status == Status::Cancelled, "Disconnect must cancel pending subscriptions.");
    const auto a = tracker.begin(Kind::Publish);
    tracker.registerMid(a, 14);
    const auto b = tracker.begin(Kind::Unsubscribe);
    tracker.registerMid(b, 15);
    expectException<MqttException>([&] { tracker.begin(Kind::Publish); });
    const auto internal = tracker.begin(Kind::Subscribe, 0, true);
    tracker.registerMid(internal, 16);
    tracker.acknowledge(Kind::Subscribe, 16, {Status::Completed, 1});
    require(tracker.peek(internal).status == Status::Unknown, "Internal restore must release its slot.");
    tracker.cancel(true);
    require(tracker.take(a).status == Status::Cancelled && tracker.take(b).status == Status::Cancelled,
            "Shutdown must cancel all outstanding operations.");
    const auto cancelledDuringSend = tracker.begin(Kind::Publish, 4);
    tracker.cancel(true);
    tracker.registerMid(cancelledDuringSend, 17);
    require(tracker.take(cancelledDuringSend).status == Status::Cancelled,
            "Cancellation racing send registration must not lose its result.");
    const auto aborted = tracker.begin(Kind::Publish, 4);
    tracker.abort(aborted);
    tracker.reset(2, 1, 4);
    require(tracker.begin(Kind::Publish, 4) > aborted, "New connection must not reuse a public token.");
}

void clientWithoutBroker() {
    MosquittoClient client;
    require(!client.isConnected() && client.state() == MqttConnectionState::Disconnected,
            "New client must be disconnected.");
    expectException<std::invalid_argument>([&] { client.publish("bad/+", {}); });
    expectException<std::invalid_argument>([&] { client.publish(std::string("a\0b", 3), {}); });
    expectException<std::invalid_argument>([&] { client.subscribe("bad/#/filter"); });
    expectException<std::invalid_argument>([&] { client.publish("valid", {}, static_cast<MqttQoS>(9)); });
    expectException<MqttException>([&] { client.publish("valid", {}); });
    expectException<MqttException>([&] { client.subscribe("valid/+"); });
    expectException<MqttException>([&] { client.unsubscribe("valid"); });
    MqttMessage out;
    require(!client.readFor(1s, out), "Disconnected client must return immediately.");
    expectException<std::invalid_argument>([&] { client.readFor(-1ms, out); });
    require(client.waitForOperation(123, 0ms).status == MqttOperationStatus::Unknown,
            "Unknown token should be explicit.");
    client.disconnect();
    client.disconnect();
    MosquittoClient moved(std::move(client));
    require(!client.isConnected() && !moved.isConnected(), "Move must preserve disconnected state.");
    client.disconnect();
    std::vector<std::future<void>> users;
    for (int i = 0; i < 8; ++i) users.push_back(std::async(std::launch::async, [] {
        for (int j = 0; j < 20; ++j) {
            MosquittoClient a;
            MosquittoClient b;
            MosquittoClient c(std::move(a));
            b = std::move(c);
        }
    }));
    for (auto& user : users) user.get();
}
}

int main() {
    try {
        configuration();
        queue();
        confirmations();
        clientWithoutBroker();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
