#pragma once

#include "mqtt/MqttErrors.h"
#include "mqtt/MqttTypes.h"
#include <limits>
#include <map>

namespace smart_home::mqtt::detail {

enum class OperationKind { Publish, Subscribe, Unsubscribe };

// Access is protected by the owning client's mutex. No lock may cross an upstream
// call: QoS 0 can invoke on_publish synchronously, under Mosquitto's callback mutex.
class OperationTracker {
public:
    struct Record {
        OperationKind kind;
        int mid = 0;
        MqttOperationResult result{MqttOperationStatus::Pending, -1};
        std::size_t bytes = 0;
        bool internal = false;
        bool registering = true;
    };
    void reset(std::size_t userLimit, std::size_t internalLimit, std::size_t byteLimit) {
        records_.clear();
        byMid_.clear();
        early_.clear();
        registrations_ = users_ = internals_ = bytes_ = 0;
        userLimit_ = userLimit;
        internalLimit_ = internalLimit;
        byteLimit_ = byteLimit;
        // nextToken_ is deliberately not reset between connections.
    }
    MqttDeliveryToken begin(OperationKind kind, std::size_t bytes = 0, bool internal = false) {
        if ((internal ? internals_ >= internalLimit_ : users_ >= userLimit_) ||
            bytes > byteLimit_ - bytes_)
            throw MqttException("MQTT pending operation or byte limit exceeded.");
        if (nextToken_ == std::numeric_limits<MqttDeliveryToken>::max())
            throw MqttException("MQTT operation token space exhausted.");
        const auto token = nextToken_++;
        records_.emplace(token, Record{kind, 0, {MqttOperationStatus::Pending, -1}, bytes, internal, true});
        ++registrations_;
        if (internal) ++internals_; else ++users_;
        bytes_ += bytes;
        return token;
    }
    void registerMid(MqttDeliveryToken token, int mid) {
        auto it = records_.find(token);
        if (it == records_.end()) return;
        auto& record = it->second;
        record.registering = false;
        --registrations_;
        record.mid = mid;
        if (record.result.status == MqttOperationStatus::Pending) {
            const Key key{record.kind, mid};
            if (byMid_.count(key)) {
                abort(token);
                throw MqttException("MQTT message ID collided with a pending operation.");
            }
            byMid_.emplace(key, token);
            const auto early = early_.find(key);
            if (early != early_.end()) {
                const auto result = early->second;
                early_.erase(early);
                complete(key, result);
            }
        } else if (record.internal) {
            erase(it);
        }
        if (registrations_ == 0) early_.clear();
    }
    void abort(MqttDeliveryToken token) {
        auto it = records_.find(token);
        if (it == records_.end()) return;
        if (it->second.registering) --registrations_;
        byMid_.erase(Key{it->second.kind, it->second.mid});
        erase(it);
        if (registrations_ == 0) early_.clear();
    }
    void acknowledge(OperationKind kind, int mid, MqttOperationResult result) {
        const Key key{kind, mid};
        if (byMid_.count(key)) complete(key, result);
        else if (registrations_ && early_.size() < userLimit_ + internalLimit_)
            early_[key] = result;
    }
    MqttOperationResult peek(MqttDeliveryToken token) const {
        const auto it = records_.find(token);
        return it == records_.end() ? MqttOperationResult{} : it->second.result;
    }
    MqttOperationResult take(MqttDeliveryToken token) {
        const auto it = records_.find(token);
        if (it == records_.end()) return {};
        const auto result = it->second.result;
        if (result.status != MqttOperationStatus::Pending && !it->second.registering)
            erase(it);
        return result;
    }
    void cancel(bool publicationsToo) {
        for (auto it = records_.begin(); it != records_.end();) {
            auto& record = it->second;
            if (record.result.status == MqttOperationStatus::Pending &&
                (publicationsToo || record.kind != OperationKind::Publish)) {
                byMid_.erase(Key{record.kind, record.mid});
                bytes_ -= record.bytes;
                record.bytes = 0;
                record.result = {MqttOperationStatus::Cancelled, -1};
            }
            if (record.internal && !record.registering &&
                record.result.status != MqttOperationStatus::Pending) {
                const auto old = it++;
                erase(old);
            } else ++it;
        }
        early_.clear();
    }
private:
    using Key = std::pair<OperationKind, int>;
    using Records = std::map<MqttDeliveryToken, Record>;
    void erase(Records::iterator it) {
        bytes_ -= it->second.bytes;
        if (it->second.internal) --internals_; else --users_;
        records_.erase(it);
    }
    void complete(const Key& key, MqttOperationResult result) {
        auto byMid = byMid_.find(key);
        auto it = records_.find(byMid->second);
        bytes_ -= it->second.bytes;
        it->second.bytes = 0;
        it->second.result = result;
        byMid_.erase(byMid);
        if (it->second.internal) erase(it);
    }
    Records records_;
    std::map<Key, MqttDeliveryToken> byMid_;
    std::map<Key, MqttOperationResult> early_;
    MqttDeliveryToken nextToken_ = 1;
    std::size_t userLimit_ = 256, internalLimit_ = 128, byteLimit_ = 16 * 1024 * 1024;
    std::size_t users_ = 0, internals_ = 0, bytes_ = 0, registrations_ = 0;
};

} // namespace smart_home::mqtt::detail
