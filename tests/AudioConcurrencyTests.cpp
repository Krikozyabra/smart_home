#include "../logging/src/Testing.h"
#include "AudioTestSupport.h"
#include <atomic>
#include <logging/Logging.h>
#include <spdlog/sinks/ostream_sink.h>
#include <sstream>
#include <system_error>

using namespace audio_test;
void checkFailureLogging() {
    namespace logging = smart_home::logging;
    std::ostringstream output;
    struct Shutdown {
        ~Shutdown() { logging::shutdown(); }
    } shutdown;
    logging::installTestSink(
        std::make_shared<spdlog::sinks::ostream_sink_mt>(output));
    {
        auto control = std::make_shared<Control>();
        auto capture = create(tiny(), control);
        control->push(-EIO);
        expectException<std::runtime_error>([&] { capture->recordFor(1ms); });
        expectException<std::runtime_error>(
            [&] { capture->rethrowIfFailed(); });
        expectException<std::runtime_error>([&] { capture->stopContinuous(); });
    }
    require(output.str().empty(),
            "observed/rethrown error is not logged again");
    {
        auto control = std::make_shared<Control>();
        auto capture = create(tiny(), control);
        control->push(-EIO);
        capture->startContinuous();
        control->await([&] { return control->finishes > 0; });
    }
    const auto logged = output.str();
    require(std::count(logged.begin(), logged.end(), '\n') == 1 &&
                logged.find("Unobserved capture failure") != std::string::npos,
            "destructor reports unobserved error exactly once");
}
void tests() {
    checkFailureLogging();
    {
        auto control = std::make_shared<Control>();
        auto capture = create(tiny(), control);
        auto recording = std::async(std::launch::async,
                                    [&] { return capture->recordFor(5ms); });
        control->awaitIdleRead(0);
        expectException<std::logic_error>([&] { capture->startContinuous(); });
        expectException<std::logic_error>([&] { capture->recordFor(1ms); });
        expectException<std::logic_error>([&] { capture->stopContinuous(); });
        require(capture->snapshot().frames.empty(), "snapshot during one-shot");
        control->push(5, ramp(5));
        require(recording.get() == ramp(5), "one-shot on caller thread");
    }
    {
        auto control = std::make_shared<Control>();
        auto capture = create(tiny(), control);
        capture->startContinuous();
        control->awaitIdleRead(0);
        expectException<std::logic_error>([&] { capture->recordFor(1ms); });
        std::vector<std::future<void>> readers;
        std::atomic<bool> go{false};
        std::mutex mutex;
        std::condition_variable changed;
        unsigned arrived = 0;
        for (int i = 0; i < 4; ++i) {
            readers.push_back(std::async(std::launch::async, [&] {
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    ++arrived;
                    changed.notify_all();
                    changed.wait(lock, [&] { return go.load(); });
                }
                for (int pass = 0; pass < 1000; ++pass) {
                    const auto snapshot = capture->snapshot();
                    require(snapshot.session_id == 1 &&
                                snapshot.frames.size() <= 3,
                            "consistent concurrent snapshot");
                    for (std::size_t j = 0; j < snapshot.frames.size(); ++j) {
                        const auto &frame = snapshot.frames[j];
                        require(
                            frame.samples ==
                                ramp(4, static_cast<int>(frame.sequence * 4)),
                            "untorn concurrent PCM");
                        if (j)
                            require(frame.sequence ==
                                        snapshot.frames[j - 1].sequence + 1,
                                    "ordered concurrent snapshot");
                    }
                }
            }));
        }
        {
            std::unique_lock<std::mutex> lock(mutex);
            require(changed.wait_for(lock, 3s, [&] { return arrived == 4; }),
                    "reader barrier");
            control->push(4000, ramp(4000));
            go = true;
            changed.notify_all();
        }
        for (auto &reader : readers)
            reader.get();
        control->awaitIdleRead(4000, 2);
        require(capture->snapshot().frames.back().sequence == 999,
                "worker made progress with readers");
        const auto before = std::chrono::steady_clock::now();
        auto stop1 =
            std::async(std::launch::async, [&] { capture->stopContinuous(); });
        auto stop2 =
            std::async(std::launch::async, [&] { capture->stopContinuous(); });
        stop1.get();
        stop2.get();
        require(std::chrono::steady_clock::now() - before < 1s,
                "stop wakes poll without waiting for 5s timeout");
        capture->stopContinuous();
        require(control->begins == 1 && control->finishes == 1,
                "one worker and one finish");
        for (auto id : control->owners)
            require(id == control->owners.front() &&
                        id != std::this_thread::get_id(),
                    "sole separate worker owns PCM");
        for (int i = 0; i < 20; ++i) {
            capture->startContinuous();
            control->awaitIdleRead(4000, 3 + i);
            capture->stopContinuous();
        }
    }
    {
        auto control = std::make_shared<Control>();
        auto capture = create(tiny(), control);
        capture->startContinuous();
        control->push(4, ramp(4));
        control->push(1, {42});
        control->push(-EPIPE);
        control->await([&] { return control->finishes > 0; });
        expectException<std::runtime_error>([&] { capture->stopContinuous(); });
        require(capture->snapshot().frames.size() == 1 &&
                    capture->snapshot().frames[0].samples == ramp(4),
                "worker failure preserves complete frame only");
        expectException<std::runtime_error>(
            [&] { capture->startContinuous(); });
        expectException<std::runtime_error>([&] { capture->stopContinuous(); });
        expectException<std::runtime_error>(
            [&] { capture->rethrowIfFailed(); });
        capture.reset();
        require(control->destroyed == 1,
                "failed worker joined before source release");
    }
    {
        auto control = std::make_shared<Control>();
        bool fail_launch = false;
        auto capture = create(tiny(), control, [&](std::function<void()> run) {
            if (fail_launch)
                throw std::system_error(std::make_error_code(
                    std::errc::resource_unavailable_try_again));
            return std::thread(std::move(run));
        });
        capture->startContinuous();
        control->push(4, ramp(4));
        control->awaitIdleRead(4);
        capture->stopContinuous();
        fail_launch = true;
        expectException<std::system_error>([&] { capture->startContinuous(); });
        require(capture->snapshot().session_id == 1 &&
                    capture->snapshot().frames.size() == 1,
                "failed launch preserves snapshot/session");
        capture->rethrowIfFailed();
        fail_launch = false;
        capture->startContinuous();
        control->awaitIdleRead(4, 2);
        capture.reset(); // Destructor must interrupt poll and join as well.
        require(control->finishes == 2 && control->destroyed == 1,
                "destructor stops live worker");
    }
}
int main() { return run(tests); }
