#pragma once

#include "oow/OOWErrors.h"
#include "oow/interfaces/IAudioInput.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace smart_home::oow {

class MockAudioInput final : public IAudioInput {
  public:
    bool simulate_fail_open = false;
    bool simulate_fail_start = false;
    bool simulate_unavailable = false;
    bool simulate_fail_read = false;
    bool simulate_fail_stop = false;
    bool simulate_eof = false;
    AudioFormat opened_format;

    void open(const AudioFormat &format) override {
        opened_format = format;
        if (simulate_fail_open)
            throw MicrophoneNotFoundException("mock open failure");
    }
    void start() override {
        if (simulate_fail_start)
            throw std::runtime_error("mock start failure");
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = false;
    }
    void stop() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
            ++stops_;
        }
        changed_.notify_all();
        if (simulate_fail_stop)
            throw MicrophoneNotFoundException("mock stop failure");
    }
    bool isAvailable() const noexcept override { return !simulate_unavailable; }
    bool readChunk(std::vector<int16_t> &chunk, size_t) override {
        std::unique_lock<std::mutex> lock(mutex_);
        ++reads_;
        changed_.notify_all();
        if (!changed_.wait_for(lock, std::chrono::seconds(3), [&] {
                return stopped_ || !chunks_.empty() || simulate_fail_read ||
                       simulate_eof;
            }))
            throw MicrophoneNotFoundException("mock read timeout");
        if (stopped_)
            return false;
        if (simulate_fail_read)
            throw MicrophoneNotFoundException("mock read failure");
        if (simulate_eof)
            return false;
        chunk = std::move(chunks_.front());
        chunks_.pop_front();
        return true;
    }
    void push(std::vector<int16_t> chunk) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            chunks_.push_back(std::move(chunk));
        }
        changed_.notify_all();
    }
    bool waitReads(size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [&] { return reads_ >= count; });
    }
    bool waitStops(size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [&] { return stops_ >= count; });
    }
    size_t stops() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stops_;
    }

  private:
    std::mutex mutex_;
    std::condition_variable changed_;
    bool stopped_ = true;
    size_t reads_ = 0;
    size_t stops_ = 0;
    std::deque<std::vector<int16_t>> chunks_;
};

} // namespace smart_home::oow
