#include "stt/SherpaSttRecognizer.h"
#include "ModelValidation.h"
#include "logging/Logging.h"

#include <sherpa-onnx/c-api/c-api.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace smart_home::stt {
namespace {

using Recognizer =
    std::unique_ptr<const SherpaOnnxOfflineRecognizer,
                    decltype(&SherpaOnnxDestroyOfflineRecognizer)>;
using Stream = std::unique_ptr<const SherpaOnnxOfflineStream,
                               decltype(&SherpaOnnxDestroyOfflineStream)>;
using Result =
    std::unique_ptr<const SherpaOnnxOfflineRecognizerResult,
                    decltype(&SherpaOnnxDestroyOfflineRecognizerResult)>;

Recognizer loadRecognizer(const SttConfig &config) {
    const auto *sdk_version = SherpaOnnxGetVersionStr();
    if (!sdk_version || std::string_view(sdk_version) != "2.0.1")
        throw std::runtime_error(
            "STT requires matching sherpa-onnx 2.0.1 headers and library.");
    detail::validateModel(config);

    SherpaOnnxOfflineRecognizerConfig sdk{};
    sdk.feat_config.sample_rate = config.sample_rate_hz;
    sdk.feat_config.feature_dim = config.feature_dim;
    sdk.model_config.nemo_ctc.model = config.model_path.c_str();
    sdk.model_config.model_type = "nemo_ctc";
    sdk.model_config.tokens = config.tokens_path.c_str();
    sdk.model_config.num_threads = config.num_threads;
    sdk.model_config.provider = "cpu";
    sdk.decoding_method = config.decoding_method.c_str();

    Recognizer recognizer(SherpaOnnxCreateOfflineRecognizer(&sdk),
                          SherpaOnnxDestroyOfflineRecognizer);
    if (!recognizer)
        throw std::runtime_error(
            "sherpa-onnx could not create the offline STT recognizer.");
    return recognizer;
}

SttResult copyResult(const SherpaOnnxOfflineRecognizerResult &sdk) {
    if (!sdk.text || sdk.count < 0 || (sdk.count > 0 && !sdk.tokens_arr))
        throw std::runtime_error("Invalid STT result from sherpa-onnx.");

    SttResult result;
    result.text = sdk.text;
    if (!detail::isValidUtf8(result.text))
        throw std::runtime_error("STT result is not valid UTF-8.");
    result.tokens.reserve(sdk.count);
    if (sdk.timestamps)
        result.timestamps.reserve(sdk.count);
    for (std::int32_t i = 0; i < sdk.count; ++i) {
        if (!sdk.tokens_arr[i] || !detail::isValidUtf8(sdk.tokens_arr[i]))
            throw std::runtime_error("Invalid STT token from sherpa-onnx.");
        result.tokens.emplace_back(sdk.tokens_arr[i]);
        if (sdk.timestamps) {
            const float timestamp = sdk.timestamps[i];
            if (!std::isfinite(timestamp) || timestamp < 0.0f)
                throw std::runtime_error(
                    "Invalid STT timestamp from sherpa-onnx.");
            result.timestamps.push_back(timestamp);
        }
    }
    return result;
}

} // namespace

struct SherpaSttRecognizer::Impl {
    explicit Impl(const SttConfig &config)
        : recognizer(loadRecognizer(config)) {
        samples.reserve(static_cast<std::size_t>(config.sample_rate_hz));
        samples.resize(detail::MinimumSamples, 0.0f);
        (void)decode(config.sample_rate_hz);
    }

    SttResult decode(int sample_rate_hz) const {
        try {
            Stream stream(SherpaOnnxCreateOfflineStream(recognizer.get()),
                          SherpaOnnxDestroyOfflineStream);
            if (!stream)
                throw std::runtime_error("Cannot create an offline stream.");

            SherpaOnnxAcceptWaveformOffline(
                stream.get(), sample_rate_hz, samples.data(),
                static_cast<std::int32_t>(samples.size()));
            SherpaOnnxDecodeOfflineStream(recognizer.get(), stream.get());
            Result result(SherpaOnnxGetOfflineStreamResult(stream.get()),
                          SherpaOnnxDestroyOfflineRecognizerResult);
            if (!result)
                throw std::runtime_error("Missing recognition result.");
            return copyResult(*result);
        } catch (const std::exception &error) {
            throw std::runtime_error(
                std::string("sherpa-onnx STT inference failed: ") +
                error.what());
        }
    }

    Recognizer recognizer;
    std::vector<float> samples;
};

SherpaSttRecognizer::SherpaSttRecognizer(SttConfig config)
    : config_(std::move(config)) {
    validateConfig(config_);
    impl_ = std::make_unique<Impl>(config_);
    logging::log(logging::Level::Debug, "STT",
                 "GigaAM-v3 CTC session warmed up.");
}

SherpaSttRecognizer::~SherpaSttRecognizer() = default;
SherpaSttRecognizer::SherpaSttRecognizer(SherpaSttRecognizer &&) noexcept =
    default;
SherpaSttRecognizer &
SherpaSttRecognizer::operator=(SherpaSttRecognizer &&) noexcept = default;

const SttConfig &SherpaSttRecognizer::getConfig() const noexcept {
    return config_;
}

SttResult SherpaSttRecognizer::recognize(const std::int16_t *pcm_data,
                                         std::size_t sample_count) {
    if (!pcm_data || sample_count == 0)
        return {};
    if (sample_count >
        static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::invalid_argument(
            "STT input exceeds the sherpa-onnx sample-count limit.");
    if (!impl_)
        throw std::logic_error("STT recognizer has been moved from.");

    auto &samples = impl_->samples;
    samples.resize(std::max(sample_count, detail::MinimumSamples));
    // A signed 16-bit integer always converts to a finite normalized float.
    std::transform(pcm_data, pcm_data + sample_count, samples.begin(),
                   [](std::int16_t sample) {
                       return static_cast<float>(sample) / 32768.0f;
                   });
    std::fill(samples.data() + sample_count, samples.data() + samples.size(),
              0.0f);
    auto result = impl_->decode(config_.sample_rate_hz);
    result.duration_seconds = static_cast<float>(
        static_cast<double>(sample_count) / config_.sample_rate_hz);
    return result;
}

} // namespace smart_home::stt
