#include "ModelValidation.h"
#include "stt/SttConfig.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace smart_home::stt::detail {
namespace {

int parseInt(std::string_view value) {
    int result = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end || result < 0)
        throw std::runtime_error(
            "Invalid integer in STT model metadata or vocabulary.");
    return result;
}

bool isBlank(std::string_view token) noexcept {
    return token == "<blk>" || token == "<blank>" || token == "<eps>";
}

void validateVocabulary(const std::string &path, int vocab_size) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open STT vocabulary: " + path);

    std::unordered_set<int> ids;
    std::unordered_set<std::string> symbols;
    bool has_blank = false;
    for (std::string line; std::getline(input, line);) {
        std::istringstream fields(line);
        std::string token, id, extra;
        if (!(fields >> token) || line.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid STT vocabulary line.");
        if (!(fields >> id)) {
            id = token;
            token =
                " "; // sherpa-onnx represents the space token with a bare ID.
        }
        if (fields >> extra || !isValidUtf8(token))
            throw std::runtime_error("Invalid STT vocabulary token.");

        const int index = parseInt(id);
        if (index >= vocab_size || !ids.insert(index).second ||
            !symbols.insert(token).second)
            throw std::runtime_error(
                "Duplicate or out-of-range STT vocabulary entry.");
        if (isBlank(token)) {
            if (index != vocab_size - 1 || has_blank)
                throw std::runtime_error("GigaAM-v3 requires the CTC blank at "
                                         "the last vocabulary ID.");
            has_blank = true;
        }
    }
    if (input.bad() || ids.size() != static_cast<std::size_t>(vocab_size) ||
        !has_blank)
        throw std::runtime_error(
            "STT vocabulary is incomplete or lacks the CTC blank.");
}

std::vector<std::int64_t> tensorShape(const Ort::TypeInfo &type,
                                      ONNXTensorElementDataType expected_type) {
    if (type.GetONNXType() != ONNX_TYPE_TENSOR)
        throw std::runtime_error(
            "STT model inputs and logits must be tensors.");
    const auto tensor = type.GetTensorTypeAndShapeInfo();
    if (tensor.GetElementType() != expected_type)
        throw std::runtime_error(
            "STT model tensor has an incompatible element type.");
    return tensor.GetShape();
}

bool acceptsBatchOne(std::int64_t dimension) {
    return dimension == 1 || dimension < 0;
}

void validateSignature(const Ort::Session &session, int feature_dim,
                       int vocab_size) {
    if (session.GetInputCount() != 2 || session.GetOutputCount() == 0)
        throw std::runtime_error(
            "GigaAM-v3 CTC needs features, lengths and logits.");

    const auto features = tensorShape(session.GetInputTypeInfo(0),
                                      ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
    const auto lengths = tensorShape(session.GetInputTypeInfo(1),
                                     ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
    const auto logits = tensorShape(session.GetOutputTypeInfo(0),
                                    ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
    if (features.size() != 3 || !acceptsBatchOne(features[0]) ||
        features[1] != feature_dim || features[2] >= 0 || lengths.size() != 1 ||
        !acceptsBatchOne(lengths[0]) || logits.size() != 3 ||
        !acceptsBatchOne(logits[0]) || logits[1] >= 0 ||
        logits[2] != vocab_size)
        throw std::runtime_error(
            "Incompatible GigaAM-v3 CTC input or output shape.");
}

void probeLogits(Ort::Session &session,
                 Ort::AllocatorWithDefaultOptions &allocator, int feature_dim,
                 int vocab_size, int subsampling) {
    // Confirm the actual output shape before the SDK's greedy decoder indexes
    // it.
    std::int64_t frames = MinimumSamples / 160;
    std::vector<float> feature_data(
        static_cast<std::size_t>(frames * feature_dim));
    const std::array<std::int64_t, 3> feature_shape{1, feature_dim, frames};
    const std::array<std::int64_t, 1> length_shape{1};
    const auto memory =
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const std::array<Ort::Value, 2> inputs{
        Ort::Value::CreateTensor<float>(
            memory, feature_data.data(), feature_data.size(),
            feature_shape.data(), feature_shape.size()),
        Ort::Value::CreateTensor<std::int64_t>(
            memory, &frames, 1, length_shape.data(), length_shape.size())};
    const auto features_name = session.GetInputNameAllocated(0, allocator);
    const auto lengths_name = session.GetInputNameAllocated(1, allocator);
    const auto logits_name = session.GetOutputNameAllocated(0, allocator);
    const std::array<const char *, 2> input_names{features_name.get(),
                                                  lengths_name.get()};
    const char *output_name = logits_name.get();
    const auto output =
        session.Run(Ort::RunOptions{nullptr}, input_names.data(), inputs.data(),
                    inputs.size(), &output_name, 1);
    const auto info = output.front().GetTensorTypeAndShapeInfo();
    const auto shape = info.GetShape();
    if (shape.size() != 3 || shape[0] != 1 || shape[1] < frames / subsampling ||
        shape[2] != vocab_size)
        throw std::runtime_error(
            "GigaAM-v3 returned incompatible logits during warmup.");
    const auto *logits = output.front().GetTensorData<float>();
    if (!std::all_of(logits, logits + info.GetElementCount(),
                     [](float value) { return std::isfinite(value); }))
        throw std::runtime_error(
            "GigaAM-v3 returned non-finite logits during warmup.");
}

} // namespace

bool isValidUtf8(std::string_view text) noexcept {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80)
            continue;
        if (lead < 0xc2 || lead > 0xf4)
            return false;
        const std::size_t length = lead < 0xe0 ? 1 : (lead < 0xf0 ? 2 : 3);
        if (text.size() - i < length)
            return false;
        const auto first = static_cast<unsigned char>(text[i]);
        if ((lead == 0xe0 && first < 0xa0) || (lead == 0xed && first > 0x9f) ||
            (lead == 0xf0 && first < 0x90) || (lead == 0xf4 && first > 0x8f))
            return false;
        for (std::size_t n = 0; n < length; ++n)
            if ((static_cast<unsigned char>(text[i++]) & 0xc0) != 0x80)
                return false;
    }
    return true;
}

void validateModel(const SttConfig &config) {
    // sherpa-onnx can terminate on invalid model metadata or tokens. Validate
    // those inputs here and release this session before constructing the SDK.
    try {
        Ort::Env environment(ORT_LOGGING_LEVEL_ERROR, "stt-validation");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(ORT_DISABLE_ALL);
        Ort::Session session(environment, config.model_path.c_str(), options);
        Ort::AllocatorWithDefaultOptions allocator;
        const auto metadata = session.GetModelMetadata();
        const auto get = [&](const char *name) {
            auto value =
                metadata.LookupCustomMetadataMapAllocated(name, allocator);
            return value ? std::string(value.get()) : std::string{};
        };
        const auto type = get("model_type");
        const auto normalization = get("normalize_type");
        if ((!type.empty() && type != "EncDecCTCModel") ||
            get("is_giga_am") != "1" ||
            (!normalization.empty() && normalization != "NA"))
            throw std::runtime_error(
                "ONNX model is not a sherpa-onnx GigaAM CTC export.");
        const int vocab_size = parseInt(get("vocab_size"));
        const int subsampling = parseInt(get("subsampling_factor"));
        if (vocab_size < 2 || subsampling != 4)
            throw std::runtime_error(
                "Invalid GigaAM-v3 vocabulary size or subsampling factor.");
        validateSignature(session, config.feature_dim, vocab_size);
        validateVocabulary(config.tokens_path, vocab_size);
        probeLogits(session, allocator, config.feature_dim, vocab_size,
                    subsampling);
    } catch (const Ort::Exception &error) {
        throw std::runtime_error(
            std::string("Cannot validate STT ONNX model: ") + error.what());
    }
}

} // namespace smart_home::stt::detail
