#pragma once

#include "mqtt/MqttTypes.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace smart_home::mqtt::detail {

class MessageQueue {
public:
    void reset(std::size_t capacity, std::size_t byteLimit) {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = capacity;
        byteLimit_ = byteLimit;
        bytes_ = 0;
        dropped_ = 0;
        messages_.clear();
        stopped_ = false;
        ++generation_;
        ready_.notify_all();
    }
    // Drop the incoming message, preserving the FIFO of messages already accepted.
    bool push(MqttMessage message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return false;
        const auto size = message.topic.size() + message.payload.size();
        if (messages_.size() >= capacity_ || size > byteLimit_ - bytes_) {
            ++dropped_;
            return false;
        }
        messages_.push_back(std::move(message));
        bytes_ += size;
        ready_.notify_one();
        return true;
    }
    void recordDrop() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        ++dropped_;
    }
    bool readFor(std::chrono::milliseconds timeout, MqttMessage& message) {
        if (timeout.count() < 0) throw std::invalid_argument("Negative MQTT read timeout.");
        std::unique_lock<std::mutex> lock(mutex_);
        const auto generation = generation_;
        ready_.wait_for(lock, timeout, [&] {
            return stopped_ || !messages_.empty() || generation_ != generation;
        });
        if (generation_ != generation || messages_.empty()) return false;
        bytes_ -= messages_.front().topic.size() + messages_.front().payload.size();
        message = std::move(messages_.front());
        messages_.pop_front();
        return true;
    }
    void stop() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        ready_.notify_all();
    }
    std::uint64_t dropped() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<MqttMessage> messages_;
    std::size_t capacity_ = 0, byteLimit_ = 0, bytes_ = 0;
    std::uint64_t dropped_ = 0, generation_ = 0;
    bool stopped_ = true;
};

} // namespace smart_home::mqtt::detail
