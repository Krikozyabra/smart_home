#include "mqtt/MosquittoClient.h"
#include "MosquittoApi.h"
#include "MosquittoGlobal.h"
#include "MessageQueue.h"
#include "OperationTracker.h"
#include "logging/Logging.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace smart_home::mqtt {
namespace {
using detail::OperationKind;
using State = MqttConnectionState;
using Status = MqttOperationStatus;

void check(int rc, const char* message) {
    if (rc != MOSQ_ERR_SUCCESS) throw MqttException(message, rc);
}
void validQoS(MqttQoS qos) {
    if (static_cast<unsigned int>(qos) > 2)
        throw std::invalid_argument("Invalid MQTT QoS.");
}
std::string validTopic(std::string_view text, bool filter) {
    if (text.empty() || text.size() > 65535 || text.find('\0') != std::string_view::npos ||
        mosquitto_validate_utf8(text.data(), static_cast<int>(text.size())) != MOSQ_ERR_SUCCESS)
        throw std::invalid_argument("Invalid MQTT topic or filter.");
    std::string topic(text);
    const int rc = filter ? mosquitto_sub_topic_check(topic.c_str())
                          : mosquitto_pub_topic_check(topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS) throw std::invalid_argument("Invalid MQTT topic or wildcard filter.");
    return topic;
}
void validTimeout(std::chrono::milliseconds timeout) {
    if (timeout.count() < 0) throw std::invalid_argument("Negative MQTT operation timeout.");
}
}

class MosquittoClient::Impl {
public:
    ~Impl() { stop(); }

    void connect(const MqttConfig& config) {
        config.validate();
        std::lock_guard<std::mutex> control(controlMutex_);
        if (transport_) throw std::logic_error("MQTT client is already started; disconnect first.");
        config_ = config;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = false;
            connack_ = 0;
            state_ = State::Connecting;
            desired_.clear();
            operations_.reset(config.maxPendingOperations, config.maxSubscriptions,
                              config.maxPendingPublishBytes);
            lastDropWarning_ = {};
        }
        queue_.reset(config.receiveQueueCapacity, config.receiveQueueBytes);
        try {
            transport_ = std::make_unique<Transport>(*this, config);
            int protocol = MQTT_PROTOCOL_V311;
            check(transport_->opts_set(MOSQ_OPT_PROTOCOL_VERSION, &protocol),
                  "Cannot set MQTT protocol version.");
            if (config.username)
                check(transport_->username_pw_set(config.username->c_str(),
                    config.password ? config.password->c_str() : nullptr),
                    "Cannot configure MQTT authentication.");
            if (config.tls) {
                const auto& tls = *config.tls;
                check(transport_->tls_set(tls.caFile.c_str(), nullptr,
                    tls.clientCertFile.empty() ? nullptr : tls.clientCertFile.c_str(),
                    tls.clientKeyFile.empty() ? nullptr : tls.clientKeyFile.c_str()),
                    "Cannot configure MQTT TLS certificates.");
                check(transport_->tls_opts_set(tls.verifyPeer ? 1 : 0, "tlsv1.2", nullptr),
                      "Cannot configure MQTT TLS verification.");
                check(transport_->tls_insecure_set(!tls.verifyPeer),
                      "Cannot configure MQTT hostname verification.");
            }
            transport_->reconnect_delay_set(config.reconnectDelayInitial, config.reconnectDelayMax, true);
            check(transport_->connect_async(config.host.c_str(), config.port, config.keepaliveSeconds),
                  "Cannot start MQTT connection.");
            check(transport_->loop_start(), "Cannot start MQTT network thread.");
            loopStarted_ = true;
            std::unique_lock<std::mutex> lock(mutex_);
            const bool completed = changed_.wait_for(lock, config.connectTimeout, [&] {
                return state_ == State::Connected || state_ == State::ConnectionFailed;
            });
            const int connack = connack_;
            const auto state = state_;
            lock.unlock();
            if (!completed) throw MqttException("MQTT connection timed out.");
            if (state != State::Connected) {
                if (connack != 0) throw MqttConnectionException(connack);
                throw MqttException("MQTT connection failed.");
            }
        } catch (...) {
            stopLocked();
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::ConnectionFailed;
            throw;
        }
    }

    void stop() noexcept {
        std::lock_guard<std::mutex> control(controlMutex_);
        stopLocked();
    }
    State state() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }
    MqttDeliveryToken publish(std::string_view text, const std::vector<std::uint8_t>& payload,
                             MqttQoS qos, bool retain) {
        validQoS(qos);
        const auto topic = validTopic(text, false);
        std::lock_guard<std::mutex> control(controlMutex_);
        if (payload.size() > config_.maxPayloadBytes)
            throw std::invalid_argument("MQTT payload exceeds configured limit.");
        return submit(OperationKind::Publish, payload.size(), false, false, [&](int* mid) {
            return transport_->publish(mid, topic.c_str(), static_cast<int>(payload.size()),
                payload.empty() ? nullptr : payload.data(), static_cast<int>(qos), retain);
        });
    }
    MqttDeliveryToken subscription(std::string_view text, std::optional<MqttQoS> qos) {
        if (qos) validQoS(*qos);
        const auto filter = validTopic(text, true);
        std::lock_guard<std::mutex> control(controlMutex_);
        std::optional<std::optional<MqttQoS>> previous;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            requireConnected();
            const auto it = desired_.find(filter);
            if (it != desired_.end()) previous.emplace(it->second);
            else if (desired_.size() >= config_.maxSubscriptions)
                throw MqttException("MQTT subscription filter limit exceeded.");
            desired_[filter] = qos;
        }
        try {
            return submit(qos ? OperationKind::Subscribe : OperationKind::Unsubscribe,
                0, false, false, [&](int* mid) {
                    return qos ? transport_->subscribe(mid, filter.c_str(), static_cast<int>(*qos))
                               : transport_->unsubscribe(mid, filter.c_str());
                });
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (previous) desired_[filter] = *previous;
            else desired_.erase(filter);
            throw;
        }
    }
    MqttOperationResult wait(MqttDeliveryToken token, std::chrono::milliseconds timeout) {
        validTimeout(timeout);
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait_for(lock, timeout, [&] {
            return operations_.peek(token).status != Status::Pending;
        });
        return operations_.take(token);
    }
    detail::MessageQueue& queue() noexcept { return queue_; }
    const detail::MessageQueue& queue() const noexcept { return queue_; }

private:
    class Transport final : public mosqpp::mosquittopp {
    public:
        Transport(Impl& owner, const MqttConfig& config)
            : mosqpp::mosquittopp(config.clientId.empty() ? nullptr : config.clientId.c_str(),
                                config.cleanSession), owner_(owner) {}
        void on_connect(int rc) override {
            owner_.callback([&] { owner_.connected(rc); });
        }
        void on_disconnect(int rc) override {
            owner_.callback([&] { owner_.disconnected(rc); });
        }
        void on_publish(int mid) override {
            owner_.callback([&] { owner_.acknowledge(OperationKind::Publish, mid, {Status::Completed, -1}); });
        }
        void on_subscribe(int mid, int count, const int* granted) override {
            owner_.callback([&] {
                const bool ok = count == 1 && granted && granted[0] >= 0 && granted[0] <= 2;
                owner_.acknowledge(OperationKind::Subscribe, mid,
                    {ok ? Status::Completed : Status::Rejected, ok ? granted[0] : -1});
                if (!ok) logging::log(logging::Level::Warn, "Mqtt", "Broker rejected a subscription.");
            });
        }
        void on_unsubscribe(int mid) override {
            owner_.callback([&] { owner_.acknowledge(OperationKind::Unsubscribe, mid, {Status::Completed, -1}); });
        }
        void on_message(const mosquitto_message* message) override {
            // Keep allocation failures inside the callback boundary. Empty payloads
            // have a null pointer; never perform pointer arithmetic on that pointer.
            try { owner_.received(message); }
            catch (...) { owner_.queue_.recordDrop(); owner_.warnDrop(); }
        }
    private:
        Impl& owner_;
    };

    template<class F> void callback(F&& action) noexcept {
        try { action(); }
        catch (...) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                state_ = State::ConnectionFailed;
                operations_.cancel(true);
                changed_.notify_all();
            }
            queue_.stop();
            // disconnect does not join. The application thread owns loop_stop.
            if (transport_) transport_->disconnect();
            logging::log(logging::Level::Error, "Mqtt", "MQTT callback processing failed.");
        }
    }
    void requireConnected() const {
        if (stopping_ || state_ != State::Connected)
            throw MqttException("MQTT client is not connected.");
    }
    template<class F> MqttDeliveryToken submit(OperationKind kind, std::size_t bytes,
                                              bool internal, bool restoring, F&& send) {
        MqttDeliveryToken token;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!restoring) requireConnected();
            else if (stopping_ || state_ != State::Connecting)
                throw MqttException("MQTT restoration was stopped.");
            token = operations_.begin(kind, bytes, internal);
        }
        int mid = 0;
        // No client lock crosses this call: an ACK can run synchronously or race
        // its return on another thread. OperationTracker stages early ACKs.
        const int rc = send(&mid);
        std::lock_guard<std::mutex> lock(mutex_);
        if (rc != MOSQ_ERR_SUCCESS) {
            operations_.abort(token);
            throw MqttException("Cannot submit MQTT operation.", rc);
        }
        try { operations_.registerMid(token, mid); }
        catch (...) { operations_.abort(token); throw; }
        changed_.notify_all();
        return token;
    }
    void acknowledge(OperationKind kind, int mid, MqttOperationResult result) {
        std::lock_guard<std::mutex> lock(mutex_);
        operations_.acknowledge(kind, mid, result);
        changed_.notify_all();
    }
    void connected(int rc) {
        std::map<std::string, std::optional<MqttQoS>> restore;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            connack_ = rc;
            if (rc != 0) state_ = State::ConnectionFailed;
            else {
                state_ = State::Connecting;
                restore = desired_;
            }
        }
        if (rc != 0) {
            // Refused credentials/protocol never enter an automatic retry loop.
            transport_->disconnect();
            queue_.stop();
            changed_.notify_all();
            return;
        }
        for (const auto& item : restore) {
            const auto& filter = item.first;
            const auto qos = item.second;
            submit(qos ? OperationKind::Subscribe : OperationKind::Unsubscribe, 0, true, true,
                [&](int* mid) {
                    return qos ? transport_->subscribe(mid, filter.c_str(), static_cast<int>(*qos))
                               : transport_->unsubscribe(mid, filter.c_str());
                });
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!stopping_) state_ = State::Connected;
            changed_.notify_all();
        }
        logging::log(logging::Level::Info, "Mqtt", "Connected to MQTT broker.");
    }
    void disconnected(int rc) {
        bool fatal = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_ || state_ == State::ConnectionFailed) {
                changed_.notify_all();
                return;
            }
            fatal = rc == MOSQ_ERR_TLS || rc == MOSQ_ERR_AUTH || rc == MOSQ_ERR_PROTOCOL;
            state_ = fatal ? State::ConnectionFailed : State::Connecting;
            operations_.cancel(fatal);
            changed_.notify_all();
        }
        if (fatal) {
            transport_->disconnect();
            queue_.stop();
        }
        logging::log(logging::Level::Warn, "Mqtt",
            fatal ? "MQTT connection failed; automatic retry stopped."
                  : "MQTT connection lost; reconnecting.");
    }
    void received(const mosquitto_message* message) {
        if (!message || !message->topic || message->payloadlen < 0 ||
            static_cast<std::size_t>(message->payloadlen) > config_.maxPayloadBytes ||
            (message->payloadlen && !message->payload)) {
            queue_.recordDrop();
            warnDrop();
            return;
        }
        MqttMessage copy;
        copy.topic = message->topic;
        copy.qos = static_cast<MqttQoS>(message->qos);
        copy.retain = message->retain;
        if (message->payloadlen > 0) {
            const auto* bytes = static_cast<const std::uint8_t*>(message->payload);
            copy.payload.assign(bytes, bytes + message->payloadlen);
        }
        if (!queue_.push(std::move(copy))) warnDrop();
    }
    void warnDrop() noexcept {
        const auto now = std::chrono::steady_clock::now();
        bool warn = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (now - lastDropWarning_ >= std::chrono::seconds(5)) {
                lastDropWarning_ = now;
                warn = true;
            }
        }
        if (warn) logging::log(logging::Level::Warn, "Mqtt", "Incoming MQTT message dropped by receive limits.");
    }
    void stopLocked() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            state_ = State::Disconnecting;
            operations_.cancel(true);
            changed_.notify_all();
        }
        queue_.stop();
        if (transport_) {
            transport_->disconnect();
            if (loopStarted_) {
                // Joining is always outside callback and queue locks. No forced
                // cancellation can unwind through C++ allocation or mutex code.
                transport_->loop_stop(false);
                loopStarted_ = false;
            }
            transport_.reset();
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Disconnected;
            changed_.notify_all();
        }
    }

    // Construction/destruction order is intentional: runtime before Transport,
    // and Transport is destroyed (including its base) before runtime cleanup.
    detail::MosquittoGlobal runtime_;
    mutable std::mutex mutex_;
    std::mutex controlMutex_;
    std::condition_variable changed_;
    State state_ = State::Disconnected;
    bool stopping_ = true, loopStarted_ = false;
    int connack_ = 0;
    MqttConfig config_;
    detail::MessageQueue queue_;
    detail::OperationTracker operations_;
    // nullopt is an unsubscribe intent, replayed even for a persistent session.
    std::map<std::string, std::optional<MqttQoS>> desired_;
    std::chrono::steady_clock::time_point lastDropWarning_{};
    std::unique_ptr<Transport> transport_;
};

MosquittoClient::MosquittoClient() : impl_(std::make_unique<Impl>()) {}
MosquittoClient::~MosquittoClient() = default;
MosquittoClient::MosquittoClient(MosquittoClient&& other) noexcept = default;
MosquittoClient& MosquittoClient::operator=(MosquittoClient&& other) noexcept = default;

void MosquittoClient::connect(const MqttConfig& config) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    impl_->connect(config);
}
void MosquittoClient::disconnect() noexcept { if (impl_) impl_->stop(); }
MqttConnectionState MosquittoClient::state() const noexcept {
    return impl_ ? impl_->state() : State::Disconnected;
}
bool MosquittoClient::isConnected() const noexcept { return state() == State::Connected; }
MqttDeliveryToken MosquittoClient::publish(std::string_view topic,
    const std::vector<std::uint8_t>& payload, MqttQoS qos, bool retain) {
    if (!impl_) throw MqttException("MQTT client was moved from.");
    return impl_->publish(topic, payload, qos, retain);
}
MqttDeliveryToken MosquittoClient::subscribe(std::string_view filter, MqttQoS qos) {
    if (!impl_) throw MqttException("MQTT client was moved from.");
    return impl_->subscription(filter, qos);
}
MqttDeliveryToken MosquittoClient::unsubscribe(std::string_view filter) {
    if (!impl_) throw MqttException("MQTT client was moved from.");
    return impl_->subscription(filter, std::nullopt);
}
bool MosquittoClient::tryRead(MqttMessage& message) { return readFor(std::chrono::milliseconds(0), message); }
bool MosquittoClient::readFor(std::chrono::milliseconds timeout, MqttMessage& message) {
    validTimeout(timeout);
    return impl_ && impl_->queue().readFor(timeout, message);
}
MqttOperationResult MosquittoClient::waitForOperation(MqttDeliveryToken token,
    std::chrono::milliseconds timeout) {
    validTimeout(timeout);
    return impl_ ? impl_->wait(token, timeout) : MqttOperationResult{};
}
bool MosquittoClient::waitForDelivery(MqttDeliveryToken token, std::chrono::milliseconds timeout) {
    return waitForOperation(token, timeout).status == Status::Completed;
}
std::uint64_t MosquittoClient::droppedMessages() const noexcept {
    return impl_ ? impl_->queue().dropped() : 0;
}

} // namespace smart_home::mqtt
