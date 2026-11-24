#include "TestUtil.h"
#include "platform/TemporaryDirectory.h"
#include "stt/SherpaSttRecognizer.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

namespace fs = std::filesystem;
using namespace smart_home::stt;

SttConfig validConfig(const fs::path &fixtures) {
    SttConfig config;
    config.model_path = (fixtures / "ctc.onnx").string();
    config.tokens_path = (fixtures / "tokens.txt").string();
    return config;
}

void writeFile(const fs::path &path, const std::string &contents) {
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << contents;
    output.close();
}

void configChecks(const fs::path &fixtures) {
    expectException<std::invalid_argument>([] { validateConfig(SttConfig{}); });
    const auto valid = validConfig(fixtures);
    require(valid.sample_rate_hz == 16000 && valid.feature_dim == 64 &&
                valid.num_threads == 2 &&
                valid.decoding_method == "greedy_search",
            "Expected the GigaAM-v3 CTC defaults");
    validateConfig(valid);
    for (const int threads : {1, 4}) {
        auto config = valid;
        config.num_threads = threads;
        validateConfig(config);
    }
    const auto reject = [&](auto change) {
        auto config = valid;
        change(config);
        expectException<std::invalid_argument>([&] { validateConfig(config); });
    };
    reject([](auto &c) { c.model_path.clear(); });
    reject([](auto &c) { c.tokens_path.clear(); });
    reject(
        [&](auto &c) { c.model_path = (fixtures / "absent.onnx").string(); });
    reject(
        [&](auto &c) { c.tokens_path = (fixtures / "absent.txt").string(); });
    reject([&](auto &c) { c.tokens_path = fixtures.string(); });
    reject([](auto &c) { c.tokens_path += '\0'; });
    reject([](auto &c) { c.sample_rate_hz = 48000; });
    reject([](auto &c) { c.feature_dim = 80; });
    reject([](auto &c) { c.num_threads = 0; });
    reject([](auto &c) { c.num_threads = -1; });
    reject([](auto &c) { c.num_threads = 5; });
    reject([](auto &c) { c.decoding_method = "modified_beam_search"; });
}

void invalidModels(const fs::path &fixtures) {
    smart_home::platform::TemporaryDirectory temp(fs::temp_directory_path(),
                                                  "smart-home-stt-");
    const auto corrupt = temp.path() / "corrupt.onnx";
    writeFile(corrupt, "not an ONNX model");
    for (const auto &path : {corrupt, fixtures / "missing_metadata.onnx",
                             fixtures / "wrong_feature_dim.onnx"}) {
        auto config = validConfig(fixtures);
        config.model_path = path.string();
        expectException<std::runtime_error>(
            [&] { SherpaSttRecognizer recognizer(config); });
    }

    auto config = validConfig(fixtures);
    const auto tokens_path = temp.path() / "tokens.txt";
    config.tokens_path = tokens_path.string();
    writeFile(tokens_path, "");
    expectException<std::invalid_argument>([&] { validateConfig(config); });
    for (const auto *contents :
         {"a 0\nb 1\nc 2\n", "a 0\nb 1\n<blk> 1\n", "a 0\nb 1\n<blk> 2 extra\n",
          "a a\nb 1\n<blk> 2\n", "a 0\nb 1\n<blk> 3\n",
          "\xff 0\nb 1\n<blk> 2\n"}) {
        writeFile(tokens_path, contents);
        expectException<std::runtime_error>(
            [&] { SherpaSttRecognizer recognizer(config); });
    }
}

std::vector<std::int16_t> signal(std::size_t count) {
    std::vector<std::int16_t> values(count);
    std::uint32_t state = 13;
    for (auto &sample : values) {
        state = state * 1664525u + 1013904223u;
        sample = static_cast<std::int16_t>(
            static_cast<std::int32_t>(state >> 16) - 32768);
    }
    values.front() = std::numeric_limits<std::int16_t>::min();
    values.back() = std::numeric_limits<std::int16_t>::max();
    return values;
}

void checkSilence(const SttResult &result, float seconds) {
    require(result.text.empty() && result.tokens.empty() &&
                result.timestamps.empty() &&
                std::abs(result.duration_seconds - seconds) < 1e-6f,
            "Expected empty recognition and the original duration");
}

void checkSpeech(const SttResult &result, float seconds) {
    require(result.text == "а" &&
                result.tokens == std::vector<std::string>{"а"} &&
                result.timestamps == std::vector<float>{0.0f},
            "Expected one UTF-8 token after CTC collapses repeated frames");
    require(std::abs(result.duration_seconds - seconds) < 1e-6f,
            "Expected duration from the original PCM16 sample count");
}

void lifetimeAndSoak(const fs::path &fixtures) {
    static_assert(!std::is_copy_constructible_v<SherpaSttRecognizer>);
    static_assert(std::is_nothrow_move_constructible_v<SherpaSttRecognizer>);
    static_assert(std::is_nothrow_move_assignable_v<SherpaSttRecognizer>);

    smart_home::platform::TemporaryDirectory temp(fs::temp_directory_path(),
                                                  "smart-home-stt-");
    const auto fixture_config = validConfig(fixtures);
    const auto config = validConfig(temp.path());
    fs::copy_file(fixture_config.model_path, config.model_path);
    fs::copy_file(fixture_config.tokens_path, config.tokens_path);
    const auto speech = signal(32000);
    const std::vector<std::int16_t> silence(413);
    SttResult retained;
    {
        SherpaSttRecognizer source(config);
        require(fs::remove(config.model_path) && fs::remove(config.tokens_path),
                "Expected to remove the loaded model and vocabulary");
        SherpaSttRecognizer moved(std::move(source));
        source = std::move(moved);
        ISttRecognizer &recognizer = source;
        require(recognizer.getConfig().model_path == config.model_path &&
                    recognizer.getConfig().tokens_path == config.tokens_path,
                "The session must retain its configuration after moving");
        checkSilence(recognizer.recognize(std::vector<std::int16_t>{}), 0.0f);
        checkSilence(recognizer.recognize(nullptr, 0), 0.0f);
        checkSilence(recognizer.recognize(nullptr, 7), 0.0f);
        checkSilence(recognizer.recognize(silence.data(), 0), 0.0f);
        checkSilence(recognizer.recognize(silence.data(), 1), 1.0f / 16000);
        expectException<std::invalid_argument>([&] {
            recognizer.recognize(speech.data(),
                                 static_cast<std::size_t>(
                                     std::numeric_limits<std::int32_t>::max()) +
                                     1);
        });
        retained = recognizer.recognize(speech.data(), speech.size());
        for (int i = 0; i < 40; ++i) {
            checkSilence(recognizer.recognize(silence), 413.0f / 16000);
            checkSpeech(recognizer.recognize(speech), 2.0f);
        }
    }
    checkSpeech(retained, 2.0f);
}

void inferenceFailure(const fs::path &fixtures) {
    auto config = validConfig(fixtures);
    config.model_path = (fixtures / "runtime_error.onnx").string();
    SherpaSttRecognizer recognizer(config);
    const auto speech = signal(16000);
    expectException<std::runtime_error>([&] { recognizer.recognize(speech); });
    checkSilence(recognizer.recognize(std::vector<std::int16_t>(1600)), 0.1f);
}

} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected STT fixture directory");
        const fs::path fixtures = argv[1];
        configChecks(fixtures);
        invalidModels(fixtures);
        lifetimeAndSoak(fixtures);
        inferenceFailure(fixtures);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
