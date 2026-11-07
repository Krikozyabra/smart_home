#include "Configuration.h"
#include "PcmSource.h"
#ifdef SMART_HOME_AUDIO_TESTING
#include "Testing.h"
#endif
#include <audio/AlsaAudioCapture.h>
#include <logging/Logging.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/eventfd.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>

namespace smart_home::audio {
namespace {
constexpr auto NoProgressTimeout = std::chrono::seconds(5);

class StopSignal {
  public:
    StopSignal() : fd_(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {
        if (fd_ < 0)
            throw std::system_error(errno, std::generic_category(),
                                    "audio eventfd");
    }
    ~StopSignal() { ::close(fd_); }
    StopSignal(const StopSignal &) = delete;
    StopSignal &operator=(const StopSignal &) = delete;
    void reset() noexcept {
        std::uint64_t value;
        while (::read(fd_, &value, sizeof(value)) < 0 && errno == EINTR) {
        }
        requested_.store(false);
    }
    void request() noexcept {
        requested_.store(true);
        const std::uint64_t value = 1;
        while (::write(fd_, &value, sizeof(value)) < 0 && errno == EINTR) {
        }
    }
    bool requested() const noexcept { return requested_.load(); }
    int descriptor() const noexcept { return fd_; }

  private:
    int fd_;
    std::atomic<bool> requested_{false};
};
} // namespace

struct AlsaAudioCapture::Impl {
    enum class Mode { Idle, Continuous, OneShot };
    const AudioConfig config;
    std::unique_ptr<detail::PcmSource> source;
    detail::ThreadLauncher launcher;
    StopSignal stop;
    std::mutex lifecycle_mutex;
    Mode mode = Mode::Idle;
    std::thread worker;
    mutable std::mutex error_mutex;
    std::exception_ptr failure;
    mutable bool failure_observed = false;
    mutable std::mutex ring_mutex;
    std::vector<AudioFrame> ring;
    PcmSamples working_frame;
    std::size_t head = 0;
    std::size_t count = 0;
    std::uint64_t session = 0;
    std::uint64_t next_sequence = 0;
    bool sequence_exhausted = false;

    Impl(AudioConfig settings, std::unique_ptr<detail::PcmSource> backend,
         detail::ThreadLauncher thread_launcher = {})
        : config(std::move(settings)), source(std::move(backend)),
          launcher(std::move(thread_launcher)),
          ring(config.buffer_capacity_frames),
          working_frame(detail::sampleCount(config, config.frame_duration)) {
        if (!source)
            throw std::invalid_argument("audio: null PCM source");
        for (auto &frame : ring)
            frame.samples.resize(working_frame.size());
        if (!launcher)
            launcher = [](std::function<void()> run) {
                return std::thread(std::move(run));
            };
    }
    ~Impl() {
        stop.request();
        if (worker.joinable())
            worker.join();
        if (failure && !failure_observed)
            logging::log(logging::Level::Error, "audio",
                         "Unobserved capture failure at destruction");
    }
    void rethrowFailure() const {
        std::exception_ptr error;
        {
            std::lock_guard<std::mutex> lock(error_mutex);
            error = failure;
            if (error)
                failure_observed = true;
        }
        if (error)
            std::rethrow_exception(error);
    }
    void saveFailure(std::exception_ptr error) {
        std::lock_guard<std::mutex> lock(error_mutex);
        failure = std::move(error);
    }
    // Only the recording owner touches source. No ring/lifecycle lock during
    // I/O.
    bool readFully(std::int16_t *destination, std::size_t frames) {
        const auto channels = static_cast<std::size_t>(config.channels);
        std::size_t completed = 0;
        auto deadline = std::chrono::steady_clock::now() + NoProgressTimeout;
        while (completed < frames) {
            if (stop.requested())
                return false;
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                throw std::runtime_error(
                    "audio: capture made no progress for 5 seconds");
            const auto result = source->read(destination + completed * channels,
                                             frames - completed);
            if (result > 0) {
                if (static_cast<std::size_t>(result) > frames - completed)
                    throw std::runtime_error(
                        "audio: PCM source returned too many frames");
                completed += static_cast<std::size_t>(result);
                deadline = std::chrono::steady_clock::now() + NoProgressTimeout;
            } else if (result == -EINTR) {
                continue;
            } else if (result == 0 || result == -EAGAIN) {
                const auto remaining =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - now);
                source->wait(stop.descriptor(),
                             static_cast<int>(
                                 std::max<std::int64_t>(1, remaining.count())));
            } else {
                throw std::runtime_error("audio: PCM read failed (errno " +
                                         std::to_string(-result) + ")");
            }
        }
        return !stop.requested();
    }
    void publish() {
        std::lock_guard<std::mutex> lock(ring_mutex);
        if (sequence_exhausted)
            throw std::runtime_error("audio: frame sequence exhausted");
        auto &slot = ring[head];
        std::copy(working_frame.begin(), working_frame.end(),
                  slot.samples.begin());
        slot.sequence = next_sequence;
        if (next_sequence == std::numeric_limits<std::uint64_t>::max())
            sequence_exhausted = true;
        else
            ++next_sequence;
        head = (head + 1) % ring.size();
        count = std::min(count + 1, ring.size());
    }
    void run() noexcept {
        try {
            source->begin();
            const auto frames = working_frame.size() /
                                static_cast<std::size_t>(config.channels);
            while (readFully(working_frame.data(), frames))
                publish();
            source->finish();
        } catch (...) {
            const auto original = std::current_exception();
            try {
                source->finish();
            } catch (...) {
            }
            saveFailure(original);
        }
    }
};

#ifdef SMART_HOME_AUDIO_TESTING
namespace {
AudioConfig checked(AudioConfig config) {
    validateConfig(config);
    return config;
}
} // namespace
#endif
AlsaAudioCapture::AlsaAudioCapture(AudioConfig config) {
    validateConfig(config);
    auto source = detail::makeAlsaSource(config);
    impl_ = std::make_unique<Impl>(std::move(config), std::move(source));
}
AlsaAudioCapture::AlsaAudioCapture(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
AlsaAudioCapture::~AlsaAudioCapture() = default;
AudioConfig AlsaAudioCapture::getConfig() const { return impl_->config; }
void AlsaAudioCapture::rethrowIfFailed() const { impl_->rethrowFailure(); }
void AlsaAudioCapture::startContinuous() {
    auto &impl = *impl_;
    std::lock_guard<std::mutex> lifecycle(impl.lifecycle_mutex);
    impl.rethrowFailure();
    if (impl.mode == Impl::Mode::OneShot)
        throw std::logic_error("audio: one-shot recording is active");
    if (impl.mode == Impl::Mode::Continuous)
        return;
    std::lock_guard<std::mutex> ring(impl.ring_mutex);
    if (impl.session == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("audio: session identifier exhausted");
    impl.stop.reset();
    // Keep the old snapshot/session if thread construction throws. The worker
    // cannot publish until the ring mutex is released after successful setup.
    impl.worker = impl.launcher([&impl] { impl.run(); });
    impl.head = impl.count = 0;
    impl.next_sequence = 0;
    impl.sequence_exhausted = false;
    ++impl.session;
    impl.mode = Impl::Mode::Continuous;
}
void AlsaAudioCapture::stopContinuous() {
    auto &impl = *impl_;
    std::lock_guard<std::mutex> lifecycle(impl.lifecycle_mutex);
    if (impl.mode == Impl::Mode::OneShot)
        throw std::logic_error("audio: one-shot recording is active");
    impl.stop.request();
    if (impl.worker.joinable())
        impl.worker.join();
    impl.mode = Impl::Mode::Idle;
    impl.rethrowFailure();
}
PcmSamples AlsaAudioCapture::recordFor(std::chrono::milliseconds duration) {
    auto &impl = *impl_;
    // Argument validation/allocation does not poison the capture state.
    std::unique_lock<std::mutex> lifecycle(impl.lifecycle_mutex);
    impl.rethrowFailure();
    if (impl.mode != Impl::Mode::Idle)
        throw std::logic_error("audio: recording is active");
    const auto samples = detail::sampleCount(impl.config, duration);
    PcmSamples result(samples);
    impl.stop.reset();
    impl.mode = Impl::Mode::OneShot;
    lifecycle.unlock();
    try {
        impl.source->begin();
        const auto channels = static_cast<std::size_t>(impl.config.channels);
        std::size_t offset = 0;
        while (offset < samples) {
            const auto chunk =
                std::min(samples - offset, impl.working_frame.size());
            impl.readFully(result.data() + offset, chunk / channels);
            offset += chunk;
        }
        impl.source->finish();
    } catch (...) {
        const auto original = std::current_exception();
        try {
            impl.source->finish();
        } catch (...) {
        }
        impl.saveFailure(original);
        lifecycle.lock();
        impl.mode = Impl::Mode::Idle;
        impl.rethrowFailure();
    }
    lifecycle.lock();
    impl.mode = Impl::Mode::Idle;
    return result;
}
AudioSnapshot AlsaAudioCapture::snapshot() const {
    const auto &impl = *impl_;
    std::lock_guard<std::mutex> lock(impl.ring_mutex);
    AudioSnapshot result;
    result.session_id = impl.session;
    result.frames.reserve(impl.count);
    const auto first =
        (impl.head + impl.ring.size() - impl.count) % impl.ring.size();
    for (std::size_t i = 0; i < impl.count; ++i)
        result.frames.push_back(impl.ring[(first + i) % impl.ring.size()]);
    return result;
}

#ifdef SMART_HOME_AUDIO_TESTING
namespace detail {
std::unique_ptr<AlsaAudioCapture>
TestAccess::create(AudioConfig config, std::unique_ptr<PcmSource> source,
                   ThreadLauncher launcher) {
    auto impl = std::make_unique<AlsaAudioCapture::Impl>(
        checked(std::move(config)), std::move(source), std::move(launcher));
    return std::unique_ptr<AlsaAudioCapture>(
        new AlsaAudioCapture(std::move(impl)));
}
void TestAccess::setCounters(AlsaAudioCapture &capture, std::uint64_t session,
                             std::uint64_t next_sequence) {
    std::lock_guard<std::mutex> lock(capture.impl_->ring_mutex);
    capture.impl_->session = session;
    capture.impl_->next_sequence = next_sequence;
}
} // namespace detail
#endif
} // namespace smart_home::audio
