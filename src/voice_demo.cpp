/** @file voice_demo.cpp
 * @brief Разовая консольная проверка обнаружения, распознавания и синтеза речи.
 */
#include <alsa/asoundlib.h>
#include <audio/AlsaAudioCapture.h>
#include <oww/OpenWakeWordDetector.h>
#include <stt/SherpaSttRecognizer.h>
#include <tts/SileroTtsSynthesizer.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {

namespace audio = smart_home::audio;
namespace oww = smart_home::oww;
namespace stt = smart_home::stt;
namespace tts = smart_home::tts;
using namespace std::chrono_literals;

struct Options {
    audio::AudioConfig capture;
    oww::OwwConfig wake;
    stt::SttConfig recognition;
    tts::TtsConfig synthesis;
    std::string playback_device = "default";
    float threshold = 0.5F;
    int record_seconds = 5;
};

void usage(std::ostream &out) {
    out << "Usage: smart_home_voice_demo --wake-model FILE --stt-model FILE\n"
           "       --stt-tokens FILE --tts-model FILE\n"
           "       [--capture-device PCM] [--playback-device PCM]\n"
           "       [--melspec FILE] [--embedding FILE] [--tts-tokens FILE]\n"
           "       [--threshold 0.5] [--record-seconds 5]\n"
           "Waits for the wake word, records one phrase, and replies in "
           "Aidar's voice.\n"
           "Both ALSA devices default to 'default'. Ctrl+C stops the demo.\n";
}

Options parseOptions(int argc, char **argv) {
    Options options;
    options.capture.sample_rate_hz = 16000;
    options.capture.channels = audio::Channels::Mono;
    options.capture.frame_duration = 80ms;
    options.synthesis.speaker = "aidar";
    for (int i = 1; i < argc; ++i) {
        const std::string_view name(argv[i]);
        if (i + 1 == argc || std::string_view(argv[i + 1]).substr(0, 2) == "--")
            throw std::invalid_argument("Missing value for " +
                                        std::string(name));
        const std::string value(argv[++i]);
        if (name == "--wake-model")
            options.wake.model_path = value;
        else if (name == "--melspec")
            options.wake.melspec_model_path = value;
        else if (name == "--embedding")
            options.wake.embedding_model_path = value;
        else if (name == "--stt-model")
            options.recognition.model_path = value;
        else if (name == "--stt-tokens")
            options.recognition.tokens_path = value;
        else if (name == "--tts-model")
            options.synthesis.model_path = value;
        else if (name == "--tts-tokens")
            options.synthesis.tokens_path = value;
        else if (name == "--capture-device" && !value.empty())
            options.capture.device_name = value;
        else if (name == "--playback-device" && !value.empty())
            options.playback_device = value;
        else if (name == "--threshold") {
            std::size_t consumed = 0;
            options.threshold = std::stof(value, &consumed);
            if (consumed != value.size() || !std::isfinite(options.threshold) ||
                options.threshold <= 0 || options.threshold > 1)
                throw std::invalid_argument("Threshold must be in (0, 1].");
        } else if (name == "--record-seconds") {
            const auto *end = value.data() + value.size();
            const auto parsed =
                std::from_chars(value.data(), end, options.record_seconds);
            if (parsed.ec != std::errc{} || parsed.ptr != end ||
                options.record_seconds < 1 || options.record_seconds > 30)
                throw std::invalid_argument("Recording duration must be an "
                                            "integer from 1 to 30 seconds.");
        } else {
            throw std::invalid_argument("Invalid option or value for " +
                                        std::string(name));
        }
    }
    if (options.wake.model_path.empty() ||
        options.recognition.model_path.empty() ||
        options.recognition.tokens_path.empty() ||
        options.synthesis.model_path.empty())
        throw std::invalid_argument(
            "Provide --wake-model, --stt-model, --stt-tokens and --tts-model.");
    return options;
}

audio::PcmSamples capturePhrase(const Options &options,
                                oww::OpenWakeWordDetector &detector) {
    audio::AlsaAudioCapture microphone(options.capture);
    const auto target_samples =
        static_cast<std::size_t>(options.record_seconds) *
        options.capture.sample_rate_hz;
    audio::PcmSamples phrase;
    phrase.reserve(target_samples);

    microphone.startContinuous();
    std::cout << "Waiting for the wake word..." << std::endl;
    std::uint64_t next_frame = 0;
    bool awakened = false;
    while (phrase.size() < target_samples) {
        microphone.rethrowIfFailed();
        const auto snapshot = microphone.snapshot();
        for (const auto &frame : snapshot.frames) {
            if (frame.sequence < next_frame)
                continue;
            if (frame.sequence != next_frame)
                throw std::runtime_error("Capture lost audio frames.");
            if (frame.samples.size() != oww::FrameSamples)
                throw std::runtime_error(
                    "Capture produced an unexpected frame size.");
            ++next_frame;

            if (!awakened) {
                const auto score = detector.processFrame(frame.samples);
                if (score >= options.threshold) {
                    awakened = true;
                    std::cout << "Wake word detected (probability " << score
                              << "). Speak now for " << options.record_seconds
                              << " seconds." << std::endl;
                }
                continue;
            }

            const auto count =
                std::min(frame.samples.size(), target_samples - phrase.size());
            phrase.insert(phrase.end(), frame.samples.data(),
                          frame.samples.data() + count);
            if (phrase.size() == target_samples)
                break;
        }
        if (phrase.size() < target_samples)
            std::this_thread::sleep_for(40ms);
    }
    std::cout << "Captured " << phrase.size() << " samples." << std::endl;
    microphone.stopContinuous();
    return phrase;
}

void checkPlayback(long status, const char *operation) {
    if (status < 0)
        throw std::runtime_error(
            std::string("ALSA playback ") + operation +
            " failed: " + snd_strerror(static_cast<int>(status)));
}

void playReply(const std::string &device, const tts::TtsAudio &audio) {
    if (audio.samples_pcm16.empty())
        throw std::runtime_error("The synthesizer returned an empty reply.");

    snd_pcm_t *handle = nullptr;
    checkPlayback(
        snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0),
        "open");
    const std::unique_ptr<snd_pcm_t, decltype(&snd_pcm_close)> pcm(
        handle, snd_pcm_close);
    checkPlayback(snd_pcm_set_params(
                      pcm.get(), SND_PCM_FORMAT_S16,
                      SND_PCM_ACCESS_RW_INTERLEAVED, 1,
                      static_cast<unsigned>(audio.sample_rate_hz), 1, 200000),
                  "setup");

    std::size_t offset = 0;
    while (offset < audio.samples_pcm16.size()) {
        const auto written =
            snd_pcm_writei(pcm.get(), audio.samples_pcm16.data() + offset,
                           audio.samples_pcm16.size() - offset);
        checkPlayback(written, "write");
        if (written == 0)
            throw std::runtime_error("ALSA playback made no progress.");
        offset += static_cast<std::size_t>(written);
    }
    checkPlayback(snd_pcm_drain(pcm.get()), "drain");
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        usage(std::cout);
        return 0;
    }

    try {
        const auto options = parseOptions(argc, argv);
        oww::OpenWakeWordDetector detector(options.wake);
        stt::SherpaSttRecognizer recognizer(options.recognition);
        tts::SileroTtsSynthesizer synthesizer(options.synthesis);

        const auto phrase = capturePhrase(options, detector);
        const auto result = recognizer.recognize(phrase);
        const auto reply = result.text.empty() ? "Я не рассл+ышал."
                                               : "Повтор+ю. " + result.text;
        std::cout << "Transcript: "
                  << (result.text.empty() ? "(empty)" : result.text) << '\n';
        std::cout << "Playing reply with Aidar..." << std::endl;
        playReply(options.playback_device, synthesizer.synthesize(reply));
    } catch (const std::exception &error) {
        std::cerr << "Voice demo failed: " << error.what() << '\n';
        return 1;
    }
}
