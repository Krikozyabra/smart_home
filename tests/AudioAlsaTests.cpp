#include "TestUtil.h"
#include <audio/AlsaAudioCapture.h>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char **argv) {
    try {
        require(argc == 2, "explicit ALSA capture PCM name required");
        smart_home::audio::AudioConfig config;
        config.device_name = argv[1];
        std::cout << "Capture PCM: " << config.device_name << "; PCM16, "
                  << config.sample_rate_hz << " Hz, "
                  << static_cast<unsigned>(config.channels) << " channel(s)\n";
        smart_home::audio::AlsaAudioCapture capture(config);
        require(capture.recordFor(std::chrono::milliseconds(100)).size() ==
                    1600,
                "real one-shot PCM size");
        capture.startContinuous();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        capture.stopContinuous();
        require(!capture.snapshot().frames.empty(), "real continuous capture");
        for (const auto &frame : capture.snapshot().frames)
            require(frame.samples.size() == 1280, "real frame size");
        config.device_name = "smart_home_nonexistent_capture_pcm";
        expectException<std::runtime_error>(
            [&] { smart_home::audio::AlsaAudioCapture invalid(config); });
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        if (argc == 2 && std::string(argv[1]).rfind("hw:", 0) == 0) {
            std::cerr << "This test requests mono PCM16 at 16000 Hz. "
                         "Raw hw PCM requires native hardware support. "
                         "To allow ALSA channel/rate/format conversion, select "
                      << "-DSMART_HOME_AUDIO_TEST_DEVICE=plug" << argv[1]
                      << '\n';
        }
        return 1;
    }
}
