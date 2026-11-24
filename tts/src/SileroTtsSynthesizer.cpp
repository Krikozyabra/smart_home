#include "tts/SileroTtsSynthesizer.h"
#include "Configuration.h"
#include "Tokenizer.h"

#include "logging/Logging.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace smart_home::tts {
namespace {

constexpr std::array<const char *, 3> InputNames{"input", "speaker",
                                                 "sample_rate"};
constexpr const char *OutputName = "output";
constexpr std::array<std::int64_t, 1> ScalarShape{1};

Ort::SessionOptions sessionOptions(int threads) {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(threads);
    options.SetInterOpNumThreads(1);
    options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.AddConfigEntry("session.intra_op.allow_spinning", "0");
    return options;
}

std::vector<std::int64_t> tensorShape(const Ort::TypeInfo &info,
                                      ONNXTensorElementDataType type,
                                      std::string_view name) {
    if (info.GetONNXType() == ONNX_TYPE_TENSOR) {
        const auto tensor = info.GetTensorTypeAndShapeInfo();
        if (tensor.GetElementType() == type)
            return tensor.GetShape();
    }
    throw std::runtime_error("TTS model has an unexpected type for " +
                             std::string(name) + '.');
}

bool allowsOne(std::int64_t dimension) {
    return dimension == 1 || dimension == -1;
}

void validateModel(const Ort::Session &session) {
    if (session.GetInputCount() != InputNames.size() ||
        session.GetOutputCount() != 1)
        throw std::runtime_error(
            "TTS model requires three inputs and one output.");

    Ort::AllocatorWithDefaultOptions allocator;
    for (std::size_t index = 0; index < InputNames.size(); ++index) {
        const auto allocated_name =
            session.GetInputNameAllocated(index, allocator);
        const std::string_view name(allocated_name.get());
        if (std::find(InputNames.begin(), InputNames.end(), name) ==
            InputNames.end())
            throw std::runtime_error("TTS model has an unexpected input: " +
                                     std::string(name));
        const auto shape =
            tensorShape(session.GetInputTypeInfo(index),
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, name);
        const bool valid =
            name == "input"
                ? shape.size() == 2 && allowsOne(shape[0]) && shape[1] == -1
                : shape.size() == 1 && allowsOne(shape[0]);
        if (!valid)
            throw std::runtime_error("TTS model has an unexpected shape for " +
                                     std::string(name) + '.');
    }

    const auto allocated_name = session.GetOutputNameAllocated(0, allocator);
    if (std::string_view(allocated_name.get()) != OutputName)
        throw std::runtime_error("TTS model has no output named 'output'.");
    const auto shape =
        tensorShape(session.GetOutputTypeInfo(0),
                    ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, OutputName);
    if (shape.size() != 2 || !allowsOne(shape[0]) || shape[1] == 0)
        throw std::runtime_error(
            "TTS model must return mono audio with shape [1, samples].");
}

TtsAudio makeAudio(const Ort::Value &output, int sample_rate_hz) {
    if (!output.IsTensor())
        throw std::runtime_error(
            "TTS model returned a value other than a tensor.");
    const auto info = output.GetTensorTypeAndShapeInfo();
    const auto shape = info.GetShape();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        shape.size() != 2 || shape[0] != 1 || shape[1] <= 0)
        throw std::runtime_error("TTS model returned an invalid audio tensor.");

    const auto count = info.GetElementCount();
    const auto *samples = output.GetTensorData<float>();
    TtsAudio audio;
    audio.sample_rate_hz = sample_rate_hz;
    audio.duration_seconds =
        static_cast<float>(static_cast<double>(count) / sample_rate_hz);
    audio.samples_float.reserve(count);
    audio.samples_pcm16.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto sample = samples[index];
        if (!std::isfinite(sample))
            throw std::runtime_error(
                "TTS model returned a non-finite audio sample.");
        const auto normalized = std::clamp(sample, -1.0f, 1.0f);
        audio.samples_float.push_back(normalized);
        audio.samples_pcm16.push_back(
            static_cast<std::int16_t>(std::lround(normalized * 32767.0f)));
    }
    return audio;
}

} // namespace

struct SileroTtsSynthesizer::Impl {
    explicit Impl(const TtsConfig &config)
        : tokenizer(config.tokens_path),
          session(env, config.model_path.c_str(),
                  sessionOptions(config.num_threads)),
          speaker(speakerId(config.speaker)),
          sample_rate(config.sample_rate_hz),
          inputs{Ort::Value{nullptr}, makeScalar(speaker),
                 makeScalar(sample_rate)} {
        validateModel(session);
    }

    TtsAudio synthesize(std::string_view text) {
        // Release the view before encode() can reallocate its backing buffer.
        inputs[0] = Ort::Value{nullptr};
        tokenizer.encode(text, tokens);
        if (tokens.empty())
            return {};
        if (tokens.size() > static_cast<std::uint64_t>(
                                std::numeric_limits<std::int64_t>::max()))
            throw std::invalid_argument("TTS text has too many tokens.");
        const std::array<std::int64_t, 2> shape{
            1, static_cast<std::int64_t>(tokens.size())};
        inputs[0] = Ort::Value::CreateTensor<std::int64_t>(
            memory, tokens.data(), tokens.size(), shape.data(), shape.size());

        Ort::Value output{nullptr};
        session.Run(Ort::RunOptions{nullptr}, InputNames.data(), inputs.data(),
                    inputs.size(), &OutputName, &output, 1);
        return makeAudio(output, static_cast<int>(sample_rate));
    }

    Ort::Value makeScalar(std::int64_t &value) const {
        return Ort::Value::CreateTensor<std::int64_t>(
            memory, &value, 1, ScalarShape.data(), ScalarShape.size());
    }

    Tokenizer tokenizer;
    Ort::Env env{ORT_LOGGING_LEVEL_FATAL, "smart_home_tts"};
    Ort::Session session;
    Ort::MemoryInfo memory =
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<std::int64_t> tokens;
    std::int64_t speaker;
    std::int64_t sample_rate;
    std::array<Ort::Value, InputNames.size()> inputs;
};

SileroTtsSynthesizer::SileroTtsSynthesizer(TtsConfig config) try
    : config_(std::move(config)) {
    validateConfig(config_);
    impl_ = std::make_unique<Impl>(config_);
    impl_->synthesize("проверка.");
    logging::log(logging::Level::Info, "tts",
                 "Silero model loaded and warmed up.");
} catch (const Ort::Exception &error) {
    throw std::runtime_error(std::string("TTS model initialization failed: ") +
                             error.what());
}

SileroTtsSynthesizer::~SileroTtsSynthesizer() = default;
SileroTtsSynthesizer::SileroTtsSynthesizer(SileroTtsSynthesizer &&) noexcept =
    default;

SileroTtsSynthesizer &
SileroTtsSynthesizer::operator=(SileroTtsSynthesizer &&other) noexcept {
    if (this != &other) {
        config_ = std::move(other.config_);
        impl_ = std::move(other.impl_);
    }
    return *this;
}

const TtsConfig &SileroTtsSynthesizer::getConfig() const noexcept {
    return config_;
}

TtsAudio SileroTtsSynthesizer::synthesize(std::string_view text) {
    if (!impl_)
        throw std::logic_error("TTS synthesizer has been moved from.");
    try {
        return impl_->synthesize(text);
    } catch (const Ort::Exception &error) {
        throw std::runtime_error(std::string("TTS inference failed: ") +
                                 error.what());
    }
}

} // namespace smart_home::tts
