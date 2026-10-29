#include "oww/OpenWakeWordDetector.h"
#include "Configuration.h"
#include "logging/Logging.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace smart_home::oww {
namespace {

constexpr std::size_t OverlapSamples = 480;
constexpr std::size_t MelBins = 32;
constexpr std::size_t MelRows = 76;
constexpr std::size_t MelStep = 8;
constexpr std::size_t EmbeddingSize = 96;
constexpr std::int64_t MaxFeatureFrames = 120;

Ort::Env &environment() {
    // Owned only by this module; no changes to another consumer's ORT
    // environment.
    static Ort::Env instance(ORT_LOGGING_LEVEL_FATAL, "smart_home_oww");
    return instance;
}

Ort::SessionOptions sessionOptions(int threads) {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(threads);
    options.SetInterOpNumThreads(1);
    options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.AddConfigEntry("session.intra_op.allow_spinning", "0");
    options.AddConfigEntry("session.inter_op.allow_spinning", "0");
    return options;
}

// Every stage has one float tensor in and one float tensor out. Keep names and
// shapes outside the frame loop; names are read from the models, not hardcoded.
class Model {
  public:
    Model(const std::string &path, const Ort::SessionOptions &options,
          const char *role)
        : session_(environment(), path.c_str(), options), role_(role) {
        if (session_.GetInputCount() != 1 || session_.GetOutputCount() != 1)
            incompatible("expected exactly one input and one output");
        const auto input_type = session_.GetInputTypeInfo(0);
        const auto output_type = session_.GetOutputTypeInfo(0);
        if (input_type.GetONNXType() != ONNX_TYPE_TENSOR ||
            output_type.GetONNXType() != ONNX_TYPE_TENSOR)
            incompatible("input and output must be tensors");
        if (input_type.GetTensorTypeAndShapeInfo().GetElementType() !=
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            output_type.GetTensorTypeAndShapeInfo().GetElementType() !=
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            incompatible("input and output must be float32");
        input_shape_ = input_type.GetTensorTypeAndShapeInfo().GetShape();
        Ort::AllocatorWithDefaultOptions allocator;
        input_name_ = session_.GetInputNameAllocated(0, allocator).get();
        output_name_ = session_.GetOutputNameAllocated(0, allocator).get();
    }

    const std::vector<std::int64_t> &inputShape() const noexcept {
        return input_shape_;
    }

    void validateInput(const std::vector<std::int64_t> &shape) const {
        if (input_shape_.size() != shape.size())
            incompatible("unexpected input rank");
        for (std::size_t i = 0; i < shape.size(); ++i)
            if (input_shape_[i] != -1 && input_shape_[i] != shape[i])
                incompatible("unexpected input dimensions");
    }

    void run(const Ort::Value &input, Ort::Value &output) {
        const char *input_name = input_name_.c_str();
        const char *output_name = output_name_.c_str();
        session_.Run(Ort::RunOptions{nullptr}, &input_name, &input, 1,
                     &output_name, &output, 1);
    }

    [[noreturn]] void incompatible(const char *reason) const {
        throw std::invalid_argument(std::string("OWW incompatible ") + role_ +
                                    " model: " + reason);
    }

  private:
    Ort::Session session_;
    const char *role_;
    std::vector<std::int64_t> input_shape_;
    std::string input_name_;
    std::string output_name_;
};

// Allocate and run once in the constructor. ORT reuses this output allocation
// on subsequent runs. Its internal arena/memory pattern is also warmed before
// use.
class Tensors {
  public:
    Tensors(Model &model, const std::vector<std::int64_t> &shape,
            const std::vector<std::int64_t> &output_shape)
        : input_(elementCount(shape), 0.0f), input_tensor_(nullptr),
          output_tensor_(nullptr) {
        model.validateInput(shape);
        const auto memory =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        input_tensor_ = Ort::Value::CreateTensor<float>(
            memory, input_.data(), input_.size(), shape.data(), shape.size());
        model.run(input_tensor_, output_tensor_);
        if (!output_tensor_.IsTensor())
            model.incompatible("output must be a tensor");
        const auto info = output_tensor_.GetTensorTypeAndShapeInfo();
        auto dimensions = info.GetShape();
        dimensions.erase(std::remove(dimensions.begin(), dimensions.end(), 1),
                         dimensions.end());
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            dimensions != output_shape)
            model.incompatible("unexpected output dimensions");
        output_ = output_tensor_.GetTensorMutableData<float>();
    }

    float *input() noexcept { return input_.data(); }
    const float *output() const noexcept { return output_; }
    void run(Model &model) { model.run(input_tensor_, output_tensor_); }

  private:
    static std::size_t elementCount(const std::vector<std::int64_t> &shape) {
        std::size_t size = 1;
        // All supplied shapes are bounded constants or validated classifier
        // context.
        for (const auto dimension : shape)
            size *= static_cast<std::size_t>(dimension);
        return size;
    }

    std::vector<float> input_;
    Ort::Value input_tensor_;
    Ort::Value output_tensor_;
    float *output_ = nullptr;
};

std::size_t featureFrames(const Model &classifier) {
    const auto &shape = classifier.inputShape();
    if (shape.size() != 3 || (shape[0] != 1 && shape[0] != -1) ||
        shape[1] < 1 || shape[1] > MaxFeatureFrames ||
        shape[2] != EmbeddingSize)
        classifier.incompatible(
            "expected [1, N, 96], with a fixed context N between 1 and 120");
    return static_cast<std::size_t>(shape[1]);
}

void requireFinite(const float *values, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i)
        if (!std::isfinite(values[i]))
            throw std::runtime_error("OWW model produced non-finite features.");
}

} // namespace

struct OpenWakeWordDetector::Impl {
    explicit Impl(OwwConfig requested)
        : config(detail::validatedConfig(std::move(requested))),
          options(sessionOptions(config.inference_threads)),
          mel(config.melspec_model_path, options, "melspectrogram"),
          embedding(config.embedding_model_path, options, "embedding"),
          classifier(config.model_path, options, "wake-word"),
          context_frames(featureFrames(classifier)),
          mel_tensors(mel, {1, FrameSamples + OverlapSamples},
                      {MelStep, MelBins}),
          embedding_tensors(embedding, {1, MelRows, MelBins, 1},
                            {EmbeddingSize}),
          classifier_tensors(
              classifier,
              {1, static_cast<std::int64_t>(context_frames), EmbeddingSize},
              {}) {
        std::fill_n(embedding_tensors.input(), MelRows * MelBins, 1.0f);
        // Deterministic silent history replaces upstream's random initial
        // history. Fill the complete mel window and then the complete
        // classifier window.
        const PcmFrame silence{};
        for (std::size_t i = 0;
             i < (MelRows + MelStep - 1) / MelStep + context_frames; ++i)
            process(silence.data());
        logging::log(logging::Level::Info, "oww",
                     "OpenWakeWord detector initialized");
    }

    float process(const std::int16_t *pcm_frame) {
        auto *audio = mel_tensors.input();
        // Retain the last 30 ms of the previous frame for the streaming mel
        // model.
        std::copy_n(audio + FrameSamples, OverlapSamples, audio);
        std::transform(
            pcm_frame, pcm_frame + FrameSamples, audio + OverlapSamples,
            [](std::int16_t value) { return static_cast<float>(value); });
        mel_tensors.run(mel);
        requireFinite(mel_tensors.output(), MelStep * MelBins);

        auto *mels = embedding_tensors.input();
        std::move(mels + MelStep * MelBins, mels + MelRows * MelBins, mels);
        std::transform(mel_tensors.output(),
                       mel_tensors.output() + MelStep * MelBins,
                       mels + (MelRows - MelStep) * MelBins,
                       [](float value) { return value / 10.0f + 2.0f; });
        embedding_tensors.run(embedding);
        requireFinite(embedding_tensors.output(), EmbeddingSize);

        auto *features = classifier_tensors.input();
        std::move(features + EmbeddingSize,
                  features + context_frames * EmbeddingSize, features);
        std::copy_n(embedding_tensors.output(), EmbeddingSize,
                    features + (context_frames - 1) * EmbeddingSize);
        classifier_tensors.run(classifier);
        const float probability = classifier_tensors.output()[0];
        if (!std::isfinite(probability) || probability < 0.0f ||
            probability > 1.0f)
            throw std::runtime_error("OWW classifier must produce a finite "
                                     "probability between 0 and 1.");
        return probability;
    }

    const OwwConfig config;
    Ort::SessionOptions options;
    Model mel;
    Model embedding;
    Model classifier;
    const std::size_t context_frames;
    Tensors mel_tensors;
    Tensors embedding_tensors;
    Tensors classifier_tensors;
};

OpenWakeWordDetector::OpenWakeWordDetector(OwwConfig config) try
    : impl_(std::make_unique<Impl>(std::move(config))) {
} catch (const Ort::Exception &error) {
    throw std::runtime_error(std::string("OWW ONNX initialization failed: ") +
                             error.what());
}

OpenWakeWordDetector::~OpenWakeWordDetector() = default;

const std::string &OpenWakeWordDetector::getModelPath() const noexcept {
    return impl_->config.model_path;
}

float OpenWakeWordDetector::processFrame(const std::int16_t *pcm_frame) {
    try {
        return impl_->process(pcm_frame);
    } catch (const Ort::Exception &error) {
        throw std::runtime_error(std::string("OWW ONNX inference failed: ") +
                                 error.what());
    }
}

} // namespace smart_home::oww
