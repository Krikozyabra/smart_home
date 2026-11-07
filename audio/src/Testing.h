#pragma once
// Private test seams; never included by public consumers.
#include "PcmSource.h"
#include <audio/AlsaAudioCapture.h>
#include <functional>
#include <thread>

namespace smart_home::audio::detail {
struct TestAccess {
    static std::unique_ptr<AlsaAudioCapture>
    create(AudioConfig config, std::unique_ptr<PcmSource> source,
           ThreadLauncher launcher = {});
    static void setCounters(AlsaAudioCapture &capture, std::uint64_t session,
                            std::uint64_t next_sequence);
};
} // namespace smart_home::audio::detail
