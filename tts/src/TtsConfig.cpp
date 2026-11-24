#include "tts/TtsConfig.h"
#include "Configuration.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace smart_home::tts {
namespace {

void validateFile(const std::string &path, const char *role) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::invalid_argument(
            std::string("TTS ") + role +
            " path must be nonempty and contain no NUL.");

    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
        throw std::invalid_argument(std::string("TTS ") + role +
                                    " must be a regular file: " + path);

    std::ifstream input(path, std::ios::binary);
    if (!input || input.peek() == std::char_traits<char>::eof())
        throw std::invalid_argument(std::string("TTS ") + role +
                                    " must be readable and nonempty: " + path);
}

} // namespace

std::int64_t speakerId(std::string_view speaker) {
    constexpr std::array<std::string_view, 6> speakers{
        "aidar", "baya", "kseniya", "xenia", "eugene", "random"};
    const auto found = std::find(speakers.begin(), speakers.end(), speaker);
    if (found == speakers.end())
        throw std::invalid_argument("Unknown TTS speaker.");
    return found - speakers.begin();
}

void validateConfig(const TtsConfig &config) {
    if (config.sample_rate_hz != 8000 && config.sample_rate_hz != 24000 &&
        config.sample_rate_hz != 48000)
        throw std::invalid_argument(
            "TTS sample rate must be 8000, 24000 or 48000 Hz.");
    if (config.num_threads < 1 || config.num_threads > 4)
        throw std::invalid_argument("TTS num_threads must be in 1..4.");
    speakerId(config.speaker);
    validateFile(config.model_path, "model");
    if (!config.tokens_path.empty())
        validateFile(config.tokens_path, "vocabulary");
}

} // namespace smart_home::tts
