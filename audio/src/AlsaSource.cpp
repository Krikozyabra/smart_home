#include "PcmSource.h"
#include <algorithm>
#include <alsa/asoundlib.h>
#include <cerrno>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace smart_home::audio::detail {
namespace {
void check(int result, std::string_view operation) {
    if (result < 0)
        throw std::runtime_error(std::string("audio ALSA ") +
                                 std::string(operation) + ": " +
                                 snd_strerror(result));
}
struct PcmCloser {
    void operator()(snd_pcm_t *pcm) const noexcept { snd_pcm_close(pcm); }
};
class AlsaSource final : public PcmSource {
  public:
    explicit AlsaSource(const AudioConfig &config) {
        snd_pcm_t *raw = nullptr;
        check(snd_pcm_open(&raw, config.device_name.c_str(),
                           SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK),
              "open capture PCM");
        pcm_.reset(raw);
        snd_pcm_hw_params_t *params_raw = nullptr;
        check(snd_pcm_hw_params_malloc(&params_raw), "allocate hw params");
        std::unique_ptr<snd_pcm_hw_params_t, decltype(&snd_pcm_hw_params_free)>
            params(params_raw, snd_pcm_hw_params_free);
        check(snd_pcm_hw_params_any(pcm_.get(), params.get()), "hw params any");
        check(snd_pcm_hw_params_set_access(pcm_.get(), params.get(),
                                           SND_PCM_ACCESS_RW_INTERLEAVED),
              "interleaved access");
        check(snd_pcm_hw_params_set_format(pcm_.get(), params.get(),
                                           SND_PCM_FORMAT_S16),
              "PCM16 format");
        check(snd_pcm_hw_params_set_channels(
                  pcm_.get(), params.get(),
                  static_cast<unsigned>(config.channels)),
              "channels=" +
                  std::to_string(static_cast<unsigned>(config.channels)));
        check(snd_pcm_hw_params_set_rate(
                  pcm_.get(), params.get(),
                  static_cast<unsigned>(config.sample_rate_hz), 0),
              "exact sample rate=" + std::to_string(config.sample_rate_hz));
        unsigned period_us = 20000;
        unsigned buffer_us = 200000;
        check(snd_pcm_hw_params_set_period_time_near(pcm_.get(), params.get(),
                                                     &period_us, nullptr),
              "period time");
        check(snd_pcm_hw_params_set_buffer_time_near(pcm_.get(), params.get(),
                                                     &buffer_us, nullptr),
              "buffer time");
        check(snd_pcm_hw_params(pcm_.get(), params.get()), "apply hw params");
        unsigned rate = 0, channels = 0;
        snd_pcm_format_t format;
        check(snd_pcm_hw_params_get_rate(params.get(), &rate, nullptr),
              "get rate");
        check(snd_pcm_hw_params_get_channels(params.get(), &channels),
              "get channels");
        check(snd_pcm_hw_params_get_format(params.get(), &format),
              "get format");
        if (rate != static_cast<unsigned>(config.sample_rate_hz) ||
            channels != static_cast<unsigned>(config.channels) ||
            format != SND_PCM_FORMAT_S16)
            throw std::runtime_error(
                "audio ALSA: negotiated PCM format differs from request");
        const int count = snd_pcm_poll_descriptors_count(pcm_.get());
        check(count, "poll descriptors count");
        if (count == 0)
            throw std::runtime_error("audio ALSA: PCM has no poll descriptors");
        descriptor_count_ = static_cast<unsigned>(count);
        descriptors_.resize(descriptor_count_ + 1);
        check(snd_pcm_poll_descriptors(pcm_.get(), descriptors_.data(),
                                       descriptor_count_),
              "poll descriptors");
    }
    void begin() override {
        check(snd_pcm_prepare(pcm_.get()), "prepare capture");
        check(snd_pcm_start(pcm_.get()), "start capture");
    }
    void finish() override { check(snd_pcm_drop(pcm_.get()), "drop capture"); }
    long read(std::int16_t *destination, std::size_t frames) override {
        const auto bounded = std::min(
            frames, static_cast<std::size_t>(
                        std::numeric_limits<snd_pcm_sframes_t>::max()));
        return snd_pcm_readi(pcm_.get(), destination, bounded);
    }
    bool wait(int stop_fd, int timeout_ms) override {
        for (auto &descriptor : descriptors_)
            descriptor.revents = 0;
        descriptors_.back() = pollfd{stop_fd, POLLIN, 0};
        const int result =
            ::poll(descriptors_.data(), descriptors_.size(), timeout_ms);
        if (result < 0) {
            if (errno == EINTR)
                return false;
            throw std::runtime_error("audio ALSA: poll failed");
        }
        if (result == 0 || descriptors_.back().revents != 0)
            return false;
        unsigned short events = 0;
        check(snd_pcm_poll_descriptors_revents(pcm_.get(), descriptors_.data(),
                                               descriptor_count_, &events),
              "poll revents");
        if (events & (POLLERR | POLLHUP | POLLNVAL))
            throw std::runtime_error(
                "audio ALSA: capture poll reported device failure");
        return (events & POLLIN) != 0;
    }

  private:
    std::unique_ptr<snd_pcm_t, PcmCloser> pcm_;
    unsigned descriptor_count_ = 0;
    std::vector<pollfd> descriptors_;
};
} // namespace
std::unique_ptr<PcmSource> makeAlsaSource(const AudioConfig &config) {
    return std::make_unique<AlsaSource>(config);
}
} // namespace smart_home::audio::detail
