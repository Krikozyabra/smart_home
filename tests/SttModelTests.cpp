#include "TestUtil.h"
#include "stt/SherpaSttRecognizer.h"

#include <sherpa-onnx/c-api/c-api.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string_view>

int main(int argc, char **argv) {
    using namespace smart_home::stt;
    using Clock = std::chrono::steady_clock;
    try {
        require(argc == 4 || argc == 5,
                "Expected model, tokens, PCM16 WAV and optional expected text");
        SttConfig config;
        config.model_path = argv[1];
        config.tokens_path = argv[2];
        const std::string_view expected = argc == 5 ? argv[4] : "";
        const std::unique_ptr<const SherpaOnnxMultiChannelWave,
                              decltype(&SherpaOnnxFreeMultiChannelWave)>
            wave(SherpaOnnxReadWaveMultiChannel(argv[3]),
                 SherpaOnnxFreeMultiChannelWave);
        require(wave && wave->num_channels == 1 && wave->num_samples > 0 &&
                    wave->sample_rate == config.sample_rate_hz,
                "Expected a nonempty mono WAV at 16000 Hz");
        std::vector<std::int16_t> pcm;
        pcm.reserve(wave->num_samples);
        for (std::int32_t i = 0; i < wave->num_samples; ++i) {
            const float value = wave->samples[0][i] * 32768.0f;
            if (!std::isfinite(value) ||
                value < std::numeric_limits<std::int16_t>::min() ||
                value > std::numeric_limits<std::int16_t>::max() ||
                std::trunc(value) != value)
                throw std::runtime_error(
                    "WAV sample is not representable as PCM16");
            pcm.push_back(static_cast<std::int16_t>(value));
        }

        const auto start = Clock::now();
        SherpaSttRecognizer recognizer(config);
        const auto initialized = Clock::now();
        const auto first = recognizer.recognize(pcm);
        const auto decoded = Clock::now();
        require(!first.text.empty() && first.duration_seconds > 0,
                "Expected a transcript");
        require(expected.empty() ||
                    first.text.find(expected) != std::string::npos,
                "Unexpected transcript: " + first.text);
        require(recognizer.recognize(pcm).text == first.text,
                "Repeated inference changed the transcript");
        const auto seconds =
            std::chrono::duration<double>(decoded - initialized).count();
        std::cout << first.text << '\n'
                  << first.duration_seconds << " s audio, " << seconds
                  << " s decode, RTF " << seconds / first.duration_seconds
                  << '\n'
                  << std::chrono::duration<double>(initialized - start).count()
                  << " s initialization and warmup\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
