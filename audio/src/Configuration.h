#pragma once
#include <audio/AudioConfig.h>

namespace smart_home::audio::detail {
std::size_t sampleCount(const AudioConfig &config,
                        std::chrono::milliseconds duration);
}
