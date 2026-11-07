#include "AudioTestSupport.h"
#include <limits>

using namespace audio_test;
void tests() {
    for (auto channels : {Channels::Mono, Channels::Stereo}) {
        auto config = tiny(channels);
        const auto count = static_cast<unsigned>(channels);
        auto control = std::make_shared<Control>();
        auto capture = create(config, control);
        const PcmSamples expected = ramp(11 * count);
        control->push(-EINTR);
        control->push(-EAGAIN);
        control->push(
            2, PcmSamples(expected.begin(), expected.begin() + 2 * count));
        control->push(0);
        control->push(9,
                      PcmSamples(expected.begin() + 2 * count, expected.end()));
        require(capture->recordFor(11ms) == expected,
                "short reads, channel order and final short chunk");
        require(capture->snapshot().frames.empty() &&
                    capture->snapshot().session_id == 0,
                "one shot leaves ring alone");
        require(control->begins == 1 && control->finishes == 1,
                "one-shot resource lifecycle");
        expectException<std::invalid_argument>(
            [&] { capture->recordFor(0ms); });
        expectException<std::invalid_argument>(
            [&] { capture->recordFor(-1ms); });
        expectException<std::invalid_argument>(
            [&] { capture->recordFor(std::chrono::milliseconds::max()); });
        capture->rethrowIfFailed();
        control->push(1, PcmSamples(count, -32768));
        require(capture->recordFor(1ms) == PcmSamples(count, -32768),
                "successful repeat after invalid argument");
        capture.reset();
        require(control->destroyed == 1, "source released");
    }
    for (int error : {EPIPE, ESTRPIPE, ENODEV, EIO}) {
        auto control = std::make_shared<Control>();
        auto capture = create(tiny(), control);
        control->push(2, {32767, -32768});
        control->push(-error);
        expectException<std::runtime_error>([&] { capture->recordFor(4ms); });
        expectException<std::runtime_error>([&] { capture->recordFor(4ms); });
        expectException<std::runtime_error>(
            [&] { capture->startContinuous(); });
        expectException<std::runtime_error>([&] { capture->stopContinuous(); });
        expectException<std::runtime_error>(
            [&] { capture->rethrowIfFailed(); });
        require(capture->snapshot().frames.empty(),
                "no partial publication on read failure");
        require(control->begins == 1 && control->finishes == 1,
                "failed recording not retried implicitly");
    }
    for (bool fail_begin : {false, true}) {
        auto control = std::make_shared<Control>();
        control->fail_begin = fail_begin;
        control->fail_finish = !fail_begin;
        auto capture = create(tiny(), control);
        control->push(4, ramp(4));
        expectException<std::runtime_error>([&] { capture->recordFor(4ms); });
        expectException<std::runtime_error>(
            [&] { capture->rethrowIfFailed(); });
        capture.reset();
        require(control->destroyed == 1, "failure releases source");
    }
    AudioConfig invalid_device;
    invalid_device.device_name = "smart_home_nonexistent_capture_pcm";
    expectException<std::runtime_error>(
        [&] { AlsaAudioCapture capture(invalid_device); });
    // No-progress failure uses the real production deadline, without busy wait.
    auto stalled_control = std::make_shared<Control>();
    auto stalled = create(tiny(), stalled_control);
    const auto before = std::chrono::steady_clock::now();
    expectException<std::runtime_error>([&] { stalled->recordFor(1ms); });
    require(std::chrono::steady_clock::now() - before >= 5s,
            "no-progress deadline");
    require(stalled_control->waits <= 3, "no active waiting without data");
}
int main() { return run(tests); }
