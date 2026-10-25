#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smart_home::oow {

constexpr uint32_t kDefaultSampleRateHz = 16000;
constexpr uint32_t kDefaultBytesPerSample = 2;
constexpr size_t kChunkSizeSamples = 1280;
constexpr float kActivationThreshold = 0.80f;
constexpr float kCalibrationThreshold = 0.65f;
constexpr size_t kRequiredCalibrationClips = 5;
constexpr size_t kIntentAudioDurationSec = 10;
constexpr size_t kIntentAudioSamplesCount =
    kDefaultSampleRateHz * kIntentAudioDurationSec;

struct AudioFormat {
    uint32_t sample_rate = kDefaultSampleRateHz;
    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
};

struct AudioClip {
    std::vector<int16_t> samples;
    AudioFormat format;
};

struct DetectionResult {
    bool detected{false};
    float confidence{0.0f};
    std::string keyword;
};

} // namespace smart_home::oow
