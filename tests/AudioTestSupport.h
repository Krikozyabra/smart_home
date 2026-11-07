#pragma once
#include "TestUtil.h"
#include "Testing.h"
#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <future>
#include <mutex>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace audio_test {
using namespace smart_home::audio;
using namespace std::chrono_literals;

struct Step {
    long frames;
    PcmSamples samples;
    std::size_t offset = 0;
};
// A controllable nonblocking PCM. The same production read loop handles its
// short reads, errno values and poll wakeup; no fake capture/ring
// implementation.
struct Control {
    const int ready_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Step> steps;
    std::size_t completed_frames = 0;
    unsigned waits = 0, begins = 0, finishes = 0, destroyed = 0;
    bool fail_begin = false, fail_finish = false;
    std::vector<std::thread::id> owners;
    Control() { require(ready_fd >= 0, "fake eventfd"); }
    ~Control() { ::close(ready_fd); }
    void push(long frames, PcmSamples samples = {}) {
        std::lock_guard<std::mutex> lock(mutex);
        steps.push_back({frames, std::move(samples), 0});
        std::uint64_t value = 1;
        require(::write(ready_fd, &value, sizeof(value)) == sizeof(value),
                "fake signal");
    }
    template <class Predicate> void await(Predicate predicate) {
        std::unique_lock<std::mutex> lock(mutex);
        require(changed.wait_for(lock, 3s, predicate),
                "timed out waiting for fake source");
    }
    void awaitIdleRead(std::size_t frames, unsigned wait_count = 1) {
        await([&] {
            return completed_frames >= frames && waits >= wait_count &&
                   steps.empty();
        });
    }
};
class Source final : public detail::PcmSource {
  public:
    Source(std::shared_ptr<Control> control, Channels channels)
        : control_(std::move(control)),
          channels_(static_cast<unsigned>(channels)) {}
    ~Source() override {
        std::lock_guard<std::mutex> lock(control_->mutex);
        ++control_->destroyed;
    }
    void begin() override {
        std::lock_guard<std::mutex> lock(control_->mutex);
        ++control_->begins;
        if (control_->fail_begin)
            throw std::runtime_error("fake format/start failure");
    }
    void finish() override {
        std::lock_guard<std::mutex> lock(control_->mutex);
        ++control_->finishes;
        control_->changed.notify_all();
        if (control_->fail_finish)
            throw std::runtime_error("fake finish failure");
    }
    long read(std::int16_t *destination, std::size_t frames) override {
        std::lock_guard<std::mutex> lock(control_->mutex);
        control_->owners.push_back(std::this_thread::get_id());
        if (control_->steps.empty())
            return -EAGAIN;
        auto &step = control_->steps.front();
        long result = step.frames;
        if (result > 0) {
            const auto take =
                std::min(frames, static_cast<std::size_t>(result));
            require(step.samples.size() >= step.offset + take * channels_,
                    "fake PCM missing");
            std::copy_n(step.samples.data() + step.offset, take * channels_,
                        destination);
            step.offset += take * channels_;
            step.frames -= static_cast<long>(take);
            control_->completed_frames += take;
            result = static_cast<long>(take);
        }
        if (step.frames <= 0) {
            control_->steps.pop_front();
            std::uint64_t value;
            require(::read(control_->ready_fd, &value, sizeof(value)) ==
                        sizeof(value),
                    "fake signal consume");
        }
        return result;
    }
    bool wait(int stop_fd, int timeout_ms) override {
        {
            std::lock_guard<std::mutex> lock(control_->mutex);
            ++control_->waits;
            control_->changed.notify_all();
        }
        pollfd descriptors[]{{control_->ready_fd, POLLIN, 0},
                             {stop_fd, POLLIN, 0}};
        const auto result = ::poll(descriptors, 2, timeout_ms);
        if (result < 0 && errno != EINTR)
            throw std::runtime_error("fake poll failure");
        return result > 0 && descriptors[0].revents != 0;
    }

  private:
    std::shared_ptr<Control> control_;
    unsigned channels_;
};
inline auto create(const AudioConfig &config,
                   const std::shared_ptr<Control> &control,
                   detail::ThreadLauncher launcher = {}) {
    return detail::TestAccess::create(
        config, std::make_unique<Source>(control, config.channels),
        std::move(launcher));
}
inline AudioConfig tiny(Channels channels = Channels::Mono) {
    AudioConfig config;
    config.sample_rate_hz = 1000;
    config.frame_duration = 4ms;
    config.buffer_capacity_frames = 3;
    config.channels = channels;
    return config;
}
inline PcmSamples ramp(std::size_t count, int offset = 0) {
    PcmSamples data(count);
    for (std::size_t i = 0; i < count; ++i)
        data[i] = static_cast<std::int16_t>(
            static_cast<int>((i + offset) % 30000) - 15000);
    return data;
}
inline int run(void (*test)()) {
    try {
        test();
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
} // namespace audio_test
