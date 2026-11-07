/** @file audio_oww_demo.cpp
 * @brief Консольная проверка захвата PCM и потокового OpenWakeWord.
 */
#include <audio/AlsaAudioCapture.h>
#include <oww/OpenWakeWordDetector.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace {
namespace audio = smart_home::audio;
namespace oww = smart_home::oww;

void usage() {
    std::cout
        << "Usage: smart_home_audio_oww_demo --model FILE [--device PCM]\n"
           "       [--melspec FILE] [--embedding FILE] [--threshold 0.5]\n"
           "Format: 16000 Hz, mono PCM16; default device: default.\n";
}
void help() {
    std::cout << "Commands: start | stop | sample (record 5s, then detect) | "
                 "help | quit\n";
}

// Один владелец детектора: консоль и инференс выполняются последовательно.
class Demo {
  public:
    Demo(audio::IAudioCapture &microphone, oww::OwwConfig config,
         float threshold)
        : microphone_(microphone), config_(std::move(config)),
          threshold_(threshold) {}

    bool command(const std::string &line) {
        std::istringstream input(line);
        std::string name, extra;
        if (!(input >> name))
            return true;
        if (input >> extra) {
            std::cout << "Commands take no arguments.\n";
        } else if (name == "start") {
            if (live_) {
                std::cout << "Already recording.\n";
            } else {
                reset();
                session_ = 0;
                last_.reset();
                microphone_.startContinuous();
                live_ = true;
                std::cout << "Live recording started.\n";
            }
        } else if (name == "stop") {
            stop();
        } else if (name == "sample") {
            if (live_) {
                std::cout << "Stop live recording before sample.\n";
            } else {
                reset();
                std::cout << "Recording 5 seconds; speak now...\n"
                          << std::flush;
                const auto pcm = microphone_.recordFor(std::chrono::seconds(5));
                if (pcm.size() != 80000)
                    throw std::runtime_error("Expected exactly 80000 samples");
                std::cout << "Recorded 80000 samples. Running OWW (last frame "
                             "padded with 640 zeros).\n";
                for (std::size_t offset = 0; offset < pcm.size();
                     offset += oww::FrameSamples) {
                    oww::PcmFrame frame{};
                    const auto size =
                        std::min(oww::FrameSamples, pcm.size() - offset);
                    std::copy_n(pcm.data() + offset, size, frame.data());
                    process(frame.data());
                }
                summary("Sample");
                detector_.reset();
            }
        } else if (name == "help") {
            help();
        } else if (name == "quit") {
            stop();
            return false;
        } else {
            std::cout << "Unknown command: " << name << '\n';
            help();
        }
        return true;
    }

    void tick() {
        if (!live_)
            return;
        microphone_.rethrowIfFailed();
        consume();
    }
    void stop() {
        if (!live_) {
            std::cout << "Recording is stopped.\n";
            return;
        }
        microphone_.stopContinuous();
        consume();
        live_ = false;
        summary("Live");
        detector_.reset();
        std::cout << "Recording stopped.\n";
    }

  private:
    void reset() {
        detector_ = std::make_unique<oww::OpenWakeWordDetector>(config_);
        frames_ = detections_ = 0;
        peak_ = 0;
        above_ = false;
    }
    void consume() {
        const auto snapshot = microphone_.snapshot();
        if (session_ != snapshot.session_id) {
            session_ = snapshot.session_id;
            last_.reset();
        }
        for (const auto &frame : snapshot.frames) {
            if (last_ && frame.sequence <= *last_)
                continue;
            if (frame.samples.size() != oww::FrameSamples)
                throw std::runtime_error(
                    "Audio frame is incompatible with OWW");
            const auto expected = last_ ? *last_ + 1 : 0;
            if (frame.sequence != expected) {
                std::cout << "Audio gap: " << frame.sequence - expected
                          << " frames lost; resetting detector context.\n";
                detector_ =
                    std::make_unique<oww::OpenWakeWordDetector>(config_);
                above_ = false;
            }
            process(frame.samples.data());
            last_ = frame.sequence;
        }
    }
    void process(const std::int16_t *pcm) {
        const float probability = detector_->processFrame(pcm);
        ++frames_;
        peak_ = std::max(peak_, probability);
        const bool above = probability >= threshold_;
        if (above && !above_) {
            ++detections_;
            std::cout << "WAKE WORD detected: probability=" << probability
                      << " frame=" << frames_ << '\n';
        }
        above_ = above;
        if (frames_ == 1 || frames_ % 12 == 0)
            std::cout << "OWW probability=" << probability
                      << " frame=" << frames_ << '\n';
    }
    void summary(const char *mode) const {
        std::cout << mode << " summary: frames=" << frames_ << " peak=" << peak_
                  << " detections=" << detections_ << '\n';
    }
    audio::IAudioCapture &microphone_;
    const oww::OwwConfig config_;
    const float threshold_;
    std::unique_ptr<oww::OpenWakeWordDetector> detector_;
    bool live_ = false;
    bool above_ = false;
    std::uint64_t session_ = 0;
    std::optional<std::uint64_t> last_;
    std::uint64_t frames_ = 0;
    std::uint64_t detections_ = 0;
    float peak_ = 0;
};

// poll/read вместо блокирующего getline: инференс продолжается без ввода.
void run(Demo &demo) {
    std::string pending;
    while (true) {
        demo.tick();
        std::cout << std::flush;
        pollfd console{STDIN_FILENO, POLLIN, 0};
        const int result = ::poll(&console, 1, 40);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            throw std::system_error(errno, std::generic_category(),
                                    "console poll");
        }
        if (console.revents & (POLLERR | POLLNVAL))
            throw std::runtime_error("Console input unavailable");
        if (!(console.revents & (POLLIN | POLLHUP)))
            continue;
        char buffer[256];
        const auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR)
                continue;
            throw std::system_error(errno, std::generic_category(),
                                    "console read");
        }
        if (count == 0) {
            if (!pending.empty() && !demo.command(pending))
                return;
            demo.stop();
            return;
        }
        pending.append(buffer, static_cast<std::size_t>(count));
        std::size_t newline;
        while ((newline = pending.find('\n')) != std::string::npos) {
            const auto line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!demo.command(line))
                return;
            std::cout << std::flush;
        }
        if (pending.size() > 4096)
            throw std::invalid_argument("Console command is too long");
    }
}
} // namespace

int main(int argc, char *argv[]) {
    try {
        audio::AudioConfig audio_config;
        oww::OwwConfig oww_config;
        float threshold = 0.5F;
        for (int i = 1; i < argc; ++i) {
            const std::string option(argv[i]);
            if (option == "--help") {
                usage();
                help();
                return 0;
            }
            if (i + 1 == argc)
                throw std::invalid_argument("Missing value for " + option);
            const std::string value(argv[++i]);
            if (option == "--model")
                oww_config.model_path = value;
            else if (option == "--device")
                audio_config.device_name = value;
            else if (option == "--melspec")
                oww_config.melspec_model_path = value;
            else if (option == "--embedding")
                oww_config.embedding_model_path = value;
            else if (option == "--threshold") {
                std::size_t end = 0;
                threshold = std::stof(value, &end);
                if (end != value.size() || !std::isfinite(threshold) ||
                    threshold <= 0 || threshold > 1)
                    throw std::invalid_argument(
                        "Threshold must be finite and in (0, 1]");
            } else
                throw std::invalid_argument("Unknown option: " + option);
        }
        oww::validateConfig(oww_config);
        audio::validateConfig(audio_config);
        audio::AlsaAudioCapture microphone(audio_config);
        Demo demo(microphone, oww_config, threshold);
        std::cout << std::fixed << std::setprecision(4)
                  << "Device: " << audio_config.device_name
                  << "; model: " << oww_config.model_path
                  << "; threshold: " << threshold << '\n';
        help();
        run(demo);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Audio/OWW demo failed: " << error.what() << '\n';
        return 1;
    }
}
