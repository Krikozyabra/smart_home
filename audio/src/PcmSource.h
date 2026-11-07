#pragma once
#include <audio/AudioConfig.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace smart_home::audio::detail {
using ThreadLauncher = std::function<std::thread(std::function<void()>)>;

// Private backend contract. Only the current recording owner calls these
// methods.
class PcmSource {
  public:
    virtual ~PcmSource() = default;
    virtual void begin() = 0;
    virtual void finish() = 0;
    // Number of ALSA frames (one value per channel), or a negative errno.
    virtual long read(std::int16_t *destination, std::size_t frames) = 0;
    // Wait for PCM or stop_fd. False means timeout, spurious event or
    // cancellation.
    virtual bool wait(int stop_fd, int timeout_ms) = 0;
};
std::unique_ptr<PcmSource> makeAlsaSource(const AudioConfig &config);
} // namespace smart_home::audio::detail
