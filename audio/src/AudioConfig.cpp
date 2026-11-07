#include "Configuration.h"
#include <audio/AudioConfig.h>
#include <audio/AudioTypes.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace smart_home::audio {
namespace detail {
std::size_t sampleCount(const AudioConfig &config,
                        std::chrono::milliseconds duration) {
    if (config.sample_rate_hz <= 0 || duration.count() <= 0 ||
        (config.channels != Channels::Mono &&
         config.channels != Channels::Stereo))
        throw std::invalid_argument(
            "audio: invalid rate, duration or channels");
    const auto rate = static_cast<std::uintmax_t>(config.sample_rate_hz);
    const auto ms = static_cast<std::uintmax_t>(duration.count());
    if (ms > std::numeric_limits<std::uintmax_t>::max() / rate)
        throw std::invalid_argument("audio: duration product overflow");
    const auto product = rate * ms;
    if (product % 1000 != 0)
        throw std::invalid_argument(
            "audio: duration requires fractional samples");
    const auto per_channel = product / 1000;
    const auto channels = static_cast<unsigned>(config.channels);
    if (per_channel > PcmSamples{}.max_size() / channels)
        throw std::invalid_argument("audio: PCM vector size overflow");
    return static_cast<std::size_t>(per_channel * channels);
}
} // namespace detail

void validateConfig(const AudioConfig &config) {
    if (config.device_name.empty() ||
        config.device_name.find('\0') != std::string::npos)
        throw std::invalid_argument("audio: invalid PCM device name");
    const auto samples = detail::sampleCount(config, config.frame_duration);
    const auto capacity = config.buffer_capacity_frames;
    if (capacity == 0 || capacity > std::vector<AudioFrame>{}.max_size() ||
        capacity > PcmSamples{}.max_size() / samples)
        throw std::invalid_argument(
            "audio: ring size overflow or zero capacity");
}
} // namespace smart_home::audio
