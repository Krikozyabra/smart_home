#include "stt/SttConfig.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace smart_home::stt {
namespace {

void validateFile(const std::string &path, const char *role) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::invalid_argument(
            std::string("STT ") + role +
            " path must be nonempty and contain no NUL.");

    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
        throw std::invalid_argument(std::string("STT ") + role +
                                    " must be a regular file: " + path);
    std::ifstream input(path, std::ios::binary);
    if (!input || input.peek() == std::char_traits<char>::eof())
        throw std::invalid_argument(std::string("STT ") + role +
                                    " must be readable and nonempty: " + path);
}

} // namespace

void validateConfig(const SttConfig &config) {
    if (config.sample_rate_hz != 16000)
        throw std::invalid_argument("STT requires mono PCM16 at 16000 Hz.");
    if (config.feature_dim != 64)
        throw std::invalid_argument(
            "STT requires 64 GigaAM-v3 Fbank features.");
    if (config.num_threads < 1 || config.num_threads > 4)
        throw std::invalid_argument("STT num_threads must be in 1..4.");
    if (config.decoding_method != "greedy_search")
        throw std::invalid_argument(
            "GigaAM-v3 CTC requires greedy_search with sherpa-onnx.");

    validateFile(config.model_path, "model");
    validateFile(config.tokens_path, "vocabulary");
}

} // namespace smart_home::stt
