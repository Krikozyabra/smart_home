#include "TestUtil.h"
#include "tts/SileroTtsSynthesizer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sys/resource.h>

namespace {

using Clock = std::chrono::steady_clock;
using namespace smart_home::tts;

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

double checkAudio(const TtsAudio &audio, int rate) {
    const auto size = audio.samples_float.size();
    const auto expected_duration = static_cast<double>(size) / rate;
    require(size > 0 && size == audio.samples_pcm16.size() &&
                audio.sample_rate_hz == rate,
            "Expected mono PCM at the configured sample rate");
    require(std::abs(audio.duration_seconds - expected_duration) < 1e-5,
            "Invalid duration for the synthesized sample count");
    require(std::all_of(audio.samples_float.begin(), audio.samples_float.end(),
                        [](float sample) {
                            return std::isfinite(sample) && sample >= -1.0f &&
                                   sample <= 1.0f;
                        }),
            "Model returned samples outside the expected range");
    require(std::any_of(audio.samples_pcm16.begin(), audio.samples_pcm16.end(),
                        [](auto sample) { return sample != 0; }),
            "The waveform is silent");
    return expected_duration;
}

} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 2 || argc == 3,
                "Expected the Silero ONNX path and optional vocabulary path");
        TtsConfig config;
        config.model_path = argv[1];
        if (argc == 3)
            config.tokens_path = argv[2];

        constexpr std::array<int, 3> rates{8000, 24000, 48000};
        constexpr std::array<const char *, 6> speakers{
            "aidar", "xenia", "baya", "kseniya", "eugene", "random"};
        constexpr std::array<const char *, 3> phrases{
            "Прив+ет, м+ир!", "Св+ет в гост+иной включён.",
            "Проверка связи завершена успешно."};
        for (std::size_t i = 0; i < speakers.size(); ++i) {
            config.sample_rate_hz = rates[i % rates.size()];
            config.speaker = speakers[i];
            const auto startup = Clock::now();
            SileroTtsSynthesizer synthesizer(config);
            const auto warmup = secondsSince(startup);
            require(synthesizer.synthesize("\t ").empty(),
                    "Blank input should yield no samples");

            double audio_seconds = 0;
            double synthesis_seconds = 0;
            for (int iteration = 0; iteration < 3; ++iteration) {
                for (auto phrase : phrases) {
                    const auto start = Clock::now();
                    const auto audio = synthesizer.synthesize(phrase);
                    synthesis_seconds += secondsSince(start);
                    audio_seconds += checkAudio(audio, config.sample_rate_hz);
                }
            }
            std::cout << config.speaker << " @ " << config.sample_rate_hz
                      << " Hz, setup " << warmup << " s, " << audio_seconds
                      << " s audio in " << synthesis_seconds << " s, RTF "
                      << synthesis_seconds / audio_seconds << '\n';
        }

        rusage usage{};
        require(getrusage(RUSAGE_SELF, &usage) == 0,
                "Could not read process peak RSS");
        std::cout << "Process peak RSS " << usage.ru_maxrss << " KiB\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
