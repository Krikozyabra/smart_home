#include "TestUtil.h"
#include "oww/OpenWakeWordDetector.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <type_traits>
#include <unistd.h>

using namespace smart_home::oww;
namespace fs = std::filesystem;

namespace {

fs::path models;

OwwConfig configFor(const char *keyword = "keyword.onnx") {
    OwwConfig config;
    config.model_path = (models / keyword).string();
    return config;
}

struct TempModels {
    TempModels() {
        root =
            fs::temp_directory_path() /
            ("smart-home-oww-" + std::to_string(::getpid()) + "-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(root);
        for (const char *name :
             {"keyword.onnx", "melspectrogram.onnx", "embedding_model.onnx"})
            fs::copy_file(models / name, root / name);
        config.model_path = (root / "keyword.onnx").string();
    }
    ~TempModels() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    fs::path root;
    OwwConfig config;
};

// An independent scalar oracle for the deliberately simple test graphs. Using
// double and row/frame means avoids reproducing production tensor manipulation.
class Reference {
  public:
    explicit Reference(std::size_t context = 16)
        : mels_(76, 2.0), features_(context, 2.0) {}

    float process(const PcmFrame &frame) {
        const double mean_pcm =
            (previous_tail_ +
             std::accumulate(frame.begin(), frame.end(), 0.0)) /
            1760.0;
        previous_tail_ = std::accumulate(frame.end() - 480, frame.end(), 0.0);
        for (int i = 0; i < 8; ++i) {
            mels_.pop_front();
            mels_.push_back(mean_pcm / 10.0 + 2.0);
        }
        features_.pop_front();
        features_.push_back(std::accumulate(mels_.begin(), mels_.end(), 0.0) /
                            mels_.size());
        const double score =
            (std::accumulate(features_.begin(), features_.end(), 0.0) /
                 features_.size() -
             2.0) /
            100.0;
        return static_cast<float>(1.0 / (1.0 + std::exp(-score)));
    }

  private:
    double previous_tail_ = 0;
    std::deque<double> mels_;
    std::deque<double> features_;
};

void checkScore(float actual, float expected) {
    require(std::isfinite(actual) && actual >= 0 && actual <= 1,
            "probability must be finite and in [0, 1]");
    require(std::abs(actual - expected) < 0.00002f,
            "pipeline probability mismatch: " + std::to_string(actual) +
                " vs " + std::to_string(expected));
}

void configurationTests() {
    const OwwConfig defaults;
    require(defaults.sample_rate_hz == 16000 && defaults.inference_threads == 1,
            "Pi defaults");
    expectException<std::invalid_argument>(
        [&] { OpenWakeWordDetector detector(defaults); });
    for (const auto &path :
         {(models / "missing.onnx").string(), models.string(),
          std::string("bad\0model.onnx", 14)}) {
        auto config = configFor();
        config.model_path = path;
        expectException<std::invalid_argument>(
            [&] { OpenWakeWordDetector detector(config); });
    }
    for (int sample_rate : {0, 8000, 44100, 48000}) {
        auto config = configFor();
        config.sample_rate_hz = sample_rate;
        expectException<std::invalid_argument>(
            [&] { OpenWakeWordDetector detector(config); });
    }
    for (int threads : {0, -1}) {
        auto config = configFor();
        config.inference_threads = threads;
        expectException<std::invalid_argument>(
            [&] { OpenWakeWordDetector detector(config); });
    }
    for (int companion = 0; companion < 2; ++companion) {
        for (const auto &path :
             {(models / "missing.onnx").string(), models.string()}) {
            auto config = configFor();
            (companion == 0 ? config.melspec_model_path
                            : config.embedding_model_path) = path;
            expectException<std::invalid_argument>(
                [&] { OpenWakeWordDetector detector(config); });
        }
    }
    validateConfig(configFor());
    {
        TempModels fixture;
        fs::remove(fixture.root / "embedding_model.onnx");
        expectException<std::invalid_argument>(
            [&] { OpenWakeWordDetector detector(fixture.config); });
    }
    {
        TempModels fixture;
        std::ofstream(fixture.config.model_path, std::ios::trunc)
            << "not an ONNX model";
        validateConfig(fixture.config); // File existence alone does not prove
                                        // it is a valid model.
        expectException<std::runtime_error>(
            [&] { OpenWakeWordDetector detector(fixture.config); });
    }
}

void incompatibleModels() {
    for (const char *name :
         {"keyword_bad_width.onnx", "keyword_dynamic_context.onnx",
          "keyword_zero_context.onnx", "keyword_large_context.onnx",
          "keyword_multiclass.onnx", "keyword_extra_output.onnx",
          "keyword_bad_type.onnx"}) {
        expectException<std::invalid_argument>(
            [&] { OpenWakeWordDetector detector(configFor(name)); });
    }
    for (const char *name :
         {"mel_bad_input.onnx", "mel_bad_output.onnx", "mel_bad_layout.onnx",
          "embedding_bad_input.onnx", "embedding_bad_output.onnx",
          "embedding_bad_layout.onnx"}) {
        auto config = configFor();
        const bool mel = std::string(name).find("mel_") == 0;
        (mel ? config.melspec_model_path : config.embedding_model_path) =
            (models / name).string();
        expectException<std::invalid_argument>(
            [&] { OpenWakeWordDetector detector(config); });
    }
}

void pipelineTests() {
    for (const auto context :
         {std::size_t{1}, std::size_t{16}, std::size_t{120}}) {
        const char *name = context == 1    ? "keyword_one_frame.onnx"
                           : context == 16 ? "keyword.onnx"
                                           : "keyword_max_context.onnx";
        OpenWakeWordDetector detector(configFor(name));
        Reference reference(context);
        IOpenWakeWord &interface = detector;
        require(interface.getModelPath() == configFor(name).model_path,
                "original classifier path");
        PcmFrame frame{};
        checkScore(interface.processFrame(frame), reference.process(frame));
        // Nonuniform PCM verifies the raw int16 scale, previous tail and both
        // histories.
        for (int i = 0; i < 180; ++i) {
            for (std::size_t sample = 0; sample < frame.size(); ++sample)
                frame[sample] = static_cast<std::int16_t>(
                    static_cast<int>((sample * 31 + i * 137) % 20001) - 10000);
            const float expected = reference.process(frame);
            if (i % 3 == 0)
                checkScore(detector.processFrame(frame.data()), expected);
            else if (i % 3 == 1)
                checkScore(detector.processFrame(frame), expected);
            else {
                std::vector<std::int16_t> vector(frame.begin(), frame.end());
                vector.push_back(123); // No length validation; only the first
                                       // 1280 samples are read.
                checkScore(interface.processFrame(vector), expected);
            }
        }
        for (const auto amplitude :
             {std::int16_t{-32768}, std::int16_t{32767}, std::int16_t{0}}) {
            frame.fill(amplitude);
            for (int i = 0; i < 140; ++i)
                checkScore(detector.processFrame(frame),
                           reference.process(frame));
        }
    }
    // Explicit feature paths need not be in the classifier directory.
    TempModels fixture;
    fixture.config.melspec_model_path =
        (models / "melspectrogram.onnx").string();
    fixture.config.embedding_model_path =
        (models / "embedding_model.onnx").string();
    fixture.config.model_path = (fixture.root / "." / "keyword.onnx").string();
    fixture.config.inference_threads = 2;
    OpenWakeWordDetector detector(fixture.config);
    require(detector.getModelPath() == fixture.config.model_path,
            "path is preserved verbatim");
    checkScore(detector.processFrame(PcmFrame{}), 0.5f);
}

void runtimeFailures() {
    for (const char *name :
         {"keyword_linear.onnx", "keyword_nan.onnx", "keyword_inf.onnx"}) {
        OpenWakeWordDetector detector(configFor(name));
        PcmFrame frame;
        frame.fill(32767);
        expectException<std::runtime_error>([&] {
            for (int i = 0; i < 160; ++i)
                detector.processFrame(frame);
        });
    }
    for (const char *name :
         {"mel_nonfinite.onnx", "embedding_nonfinite.onnx"}) {
        auto config = configFor();
        const bool mel = std::string(name).find("mel_") == 0;
        (mel ? config.melspec_model_path : config.embedding_model_path) =
            (models / name).string();
        OpenWakeWordDetector detector(config);
        PcmFrame frame;
        frame.fill(32767);
        expectException<std::runtime_error>(
            [&] { detector.processFrame(frame); });
    }
    OpenWakeWordDetector negative(configFor("keyword_linear.onnx"));
    PcmFrame frame;
    frame.fill(-32768);
    expectException<std::runtime_error>([&] {
        for (int i = 0; i < 160; ++i)
            negative.processFrame(frame);
    });
}

void lifetimeAndSoak() {
    TempModels fixture;
    OpenWakeWordDetector first(fixture.config);
    OpenWakeWordDetector second(fixture.config);
    fs::remove_all(
        fixture.root); // No model reload or file I/O during frame processing.
    Reference reference;
    PcmFrame frame;
    for (int i = 0; i < 3000; ++i) {
        frame.fill(static_cast<std::int16_t>((i * 719) % 60001 - 30000));
        checkScore(first.processFrame(frame), reference.process(frame));
        checkScore(second.processFrame(PcmFrame{}),
                   0.5f); // Independent stream history.
    }
}

} // namespace

int main(int argc, char **argv) {
    static_assert(
        std::is_same_v<decltype(std::declval<IOpenWakeWord &>().processFrame(
                           PcmFrame{})),
                       float>);
    static_assert(
        noexcept(std::declval<const IOpenWakeWord &>().getModelPath()));
    try {
        require(argc == 2, "expected test models directory");
        models = argv[1];
        configurationTests();
        incompatibleModels();
        pipelineTests();
        runtimeFailures();
        lifetimeAndSoak();
        std::cout << "OWW configuration, ONNX pipeline, failures and soak "
                     "tests passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
