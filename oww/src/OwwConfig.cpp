#include "oww/OwwConfig.h"
#include "Configuration.h"

#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace smart_home::oww {
namespace {

void validateModelFile(const std::string &path, const char *role) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::invalid_argument(
            std::string("OWW ") + role +
            " path must be nonempty and contain no NUL.");
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error)
        throw std::invalid_argument(
            std::string("OWW ") + role +
            " model is not an accessible regular file: " + path);
}

} // namespace

OwwConfig detail::validatedConfig(OwwConfig config) {
    validateModelFile(config.model_path, "wake-word");
    if (config.sample_rate_hz != 16000)
        throw std::invalid_argument(
            "OWW requires 16000 Hz mono PCM16; resampling is the caller's "
            "responsibility.");
    if (config.inference_threads < 1)
        throw std::invalid_argument("OWW inference_threads must be positive.");

    const auto directory =
        std::filesystem::path(config.model_path).parent_path();
    if (config.melspec_model_path.empty())
        config.melspec_model_path =
            (directory / "melspectrogram.onnx").string();
    if (config.embedding_model_path.empty())
        config.embedding_model_path =
            (directory / "embedding_model.onnx").string();
    validateModelFile(config.melspec_model_path, "melspectrogram");
    validateModelFile(config.embedding_model_path, "embedding");
    return config;
}

void validateConfig(const OwwConfig &config) {
    (void)detail::validatedConfig(config);
}

} // namespace smart_home::oww
