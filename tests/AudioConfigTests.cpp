#include "AudioTestSupport.h"
#include "Configuration.h"
#include <limits>
#include <oww/IOpenWakeWord.h>

using namespace audio_test;
void tests() {
    AudioConfig config;
    validateConfig(config);
    require(config.device_name == "default" && config.sample_rate_hz == 16000 &&
                config.channels == Channels::Mono &&
                config.frame_duration == 80ms &&
                config.buffer_capacity_frames == 125,
            "defaults");
    require(detail::sampleCount(config, config.frame_duration) ==
                smart_home::oww::FrameSamples,
            "OWW frame contract");
    require(detail::sampleCount(config, config.frame_duration) * 125 *
                    sizeof(std::int16_t) ==
                320000,
            "ring bytes");
    auto reject = [](auto modify) {
        AudioConfig invalid;
        modify(invalid);
        expectException<std::invalid_argument>(
            [&] { validateConfig(invalid); });
    };
    reject([](auto &c) { c.device_name.clear(); });
    reject([](auto &c) { c.device_name = std::string("a\0b", 3); });
    reject([](auto &c) { c.sample_rate_hz = 0; });
    reject([](auto &c) { c.sample_rate_hz = -1; });
    reject([](auto &c) { c.channels = static_cast<Channels>(3); });
    reject([](auto &c) { c.frame_duration = 0ms; });
    reject([](auto &c) { c.frame_duration = -1ms; });
    reject([](auto &c) { c.buffer_capacity_frames = 0; });
    reject([](auto &c) {
        c.buffer_capacity_frames = std::numeric_limits<std::size_t>::max();
    });
    reject([](auto &c) {
        c.sample_rate_hz = 44100;
        c.frame_duration = 1ms;
    });
    reject([](auto &c) {
        c.sample_rate_hz = std::numeric_limits<int>::max();
        c.frame_duration = std::chrono::milliseconds::max();
    });
    reject([](auto &c) {
        c.sample_rate_hz = 1000;
        c.frame_duration = std::chrono::milliseconds::max();
    });
    config.sample_rate_hz = 44100;
    config.frame_duration = 10ms;
    config.channels = Channels::Stereo;
    validateConfig(config);
    require(detail::sampleCount(config, 10ms) == 882, "exact stereo duration");
    reject([](auto &c) {
        c.buffer_capacity_frames = PcmSamples{}.max_size() / 1280 + 1;
    });
    auto control = std::make_shared<Control>();
    auto capture = create(config, control);
    auto copy = capture->getConfig();
    require(copy.device_name == config.device_name &&
                copy.channels == config.channels &&
                copy.sample_rate_hz == config.sample_rate_hz &&
                copy.frame_duration == config.frame_duration &&
                copy.buffer_capacity_frames == config.buffer_capacity_frames,
            "all settings preserved");
    expectException<std::invalid_argument>([&] { capture->recordFor(1ms); });
    capture->rethrowIfFailed();
    copy.device_name = "changed";
    require(capture->getConfig().device_name == "default",
            "settings owned copy");
}
int main() { return run(tests); }
