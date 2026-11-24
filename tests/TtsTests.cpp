#include "TestUtil.h"
#include "platform/TemporaryDirectory.h"
#include "tts/SileroTtsSynthesizer.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>
#include <utility>

namespace {

namespace fs = std::filesystem;
using namespace smart_home::tts;

constexpr char CustomVocabulary[] =
    "\xef\xbb\xbf_\r\n~\r\nё\r\n.\r\n+\r\n \r\nа\r\nq\r\n";

TtsConfig configFor(const fs::path &model) {
    TtsConfig config;
    config.model_path = model.string();
    return config;
}

void writeFile(const fs::path &path, std::string_view contents) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << contents;
    file.close();
}

void checkEmpty(const TtsAudio &audio) {
    require(audio.empty() && audio.sample_rate_hz == 0 &&
                audio.duration_seconds == 0.0f,
            "Expected an empty audio buffer and zero duration");
}

void checkEcho(const TtsAudio &audio, const std::vector<int> &tokens,
               int speaker = 3, int rate = 24000) {
    constexpr std::array<float, 7> levels{-1, -1, -0.5f, 0, 0.5f, 1, 1};
    constexpr std::array<int, 7> pcm_levels{-32767, -32767, -16384, 0,
                                            16384,  32767,  32767};
    const auto count = tokens.size() + 2 + levels.size();
    require(!audio.empty() && audio.samples_float.size() == count &&
                audio.samples_pcm16.size() == count &&
                audio.sample_rate_hz == rate,
            "Expected both PCM formats and the configured rate");
    require(std::abs(audio.duration_seconds -
                     static_cast<double>(count) / rate) < 1e-7,
            "Expected duration from the PCM count and configured sample rate");

    for (std::size_t i = 0; i < tokens.size(); ++i)
        require(audio.samples_float[i] == static_cast<float>(tokens[i]) / 64,
                "Unexpected ONNX token at index " + std::to_string(i));
    require(audio.samples_float[tokens.size()] ==
                    static_cast<float>(speaker) / 8 &&
                std::abs(audio.samples_float[tokens.size() + 1] -
                         static_cast<float>(rate) / 48000) < 1e-6f,
            "Unexpected speaker or sample rate sent to ONNX");
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const auto index = tokens.size() + 2 + i;
        require(audio.samples_float[index] == levels[i] &&
                    audio.samples_pcm16[index] == pcm_levels[i],
                "Unexpected saturation or rounding at index " +
                    std::to_string(i));
    }
}

void checkConfig(const fs::path &model, const fs::path &temp) {
    const auto valid = configFor(model);
    require(valid.sample_rate_hz == 24000 && valid.speaker == "xenia" &&
                valid.num_threads == 2 && valid.tokens_path.empty(),
            "Unexpected TTS defaults");
    validateConfig(valid);
    for (int threads : {1, 4}) {
        auto config = valid;
        config.num_threads = threads;
        validateConfig(config);
    }

    const auto reject = [&](auto change) {
        auto config = valid;
        change(config);
        expectException<std::invalid_argument>([&] { validateConfig(config); });
    };
    const auto empty = temp / "empty";
    writeFile(empty, "");
    reject([](auto &c) { c.model_path.clear(); });
    reject([](auto &c) { c.model_path += '\0'; });
    reject([&](auto &c) { c.model_path = (temp / "absent.onnx").string(); });
    reject([&](auto &c) { c.model_path = empty.string(); });
    reject([&](auto &c) { c.model_path = temp.string(); });
    reject([&](auto &c) { c.tokens_path = (temp / "absent.txt").string(); });
    reject([&](auto &c) { c.tokens_path = empty.string(); });
    reject([&](auto &c) { c.tokens_path = temp.string(); });
    reject([](auto &c) { c.tokens_path = std::string("tokens\0.txt", 11); });
    reject([](auto &c) { c.speaker = "Xenia"; });
    reject([](auto &c) { c.speaker.clear(); });
    reject([](auto &c) { c.sample_rate_hz = 16000; });
    reject([](auto &c) { c.num_threads = 0; });
    reject([](auto &c) { c.num_threads = 5; });
}

void checkText(const fs::path &model) {
    SileroTtsSynthesizer synthesizer(configFor(model));
    ITtsSynthesizer &tts = synthesizer;
    checkEmpty(tts.synthesize(std::string_view{}));
    checkEmpty(tts.synthesize("\t\r\n\v\f    　"));
    checkEmpty(tts.synthesize("ABC123 🙂_~+"));

    // These IDs are the positions in the specified built-in alphabet.
    checkEcho(tts.synthesize("\t+Ёж\u00a0\n М+Ир123🙂 ~_ "),
              {36, 8, 9, 35, 15, 36, 11, 19, 37, 1});
    checkEcho(tts.synthesize(std::string("А!")), {2, 39, 1});
    checkEcho(tts.synthesize("А?"), {2, 40, 1});
    checkEcho(tts.synthesize("А."), {2, 37, 1});
    checkEcho(tts.synthesize("+Б + А++"), {3, 35, 2, 37, 1});
    for (const auto *invalid :
         {"\xff", "\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf0\x9f",
          "\xf4\x90\x80\x80", "а\xd0."})
        expectException<std::invalid_argument>(
            [&] { tts.synthesize(invalid); });
    expectException<std::invalid_argument>([&] { tts.synthesize(nullptr); });
    checkEcho(tts.synthesize("а"), {2, 37, 1});
}

void checkOptions(const fs::path &model) {
    constexpr std::array<const char *, 6> speakers{"aidar", "baya",   "kseniya",
                                                   "xenia", "eugene", "random"};
    constexpr std::array<int, 3> rates{8000, 24000, 48000};
    for (std::size_t i = 0; i < speakers.size(); ++i) {
        auto config = configFor(model);
        config.speaker = speakers[i];
        config.sample_rate_hz = rates[i % rates.size()];
        SileroTtsSynthesizer synthesizer(config);
        checkEcho(synthesizer.synthesize("а"), {2, 37, 1}, static_cast<int>(i),
                  config.sample_rate_hz);
    }
}

void checkModels(const fs::path &fixtures, const fs::path &temp) {
    const auto corrupt = temp / "corrupt.onnx";
    writeFile(corrupt, "not ONNX");
    for (const auto &path :
         {corrupt, fixtures / "wrong_type.onnx", fixtures / "wrong_name.onnx",
          fixtures / "wrong_rank.onnx", fixtures / "fixed_input.onnx",
          fixtures / "warm_failure.onnx"})
        expectException<std::runtime_error>(
            [&] { SileroTtsSynthesizer synthesizer(configFor(path)); });

    SileroTtsSynthesizer nonfinite(configFor(fixtures / "late_failure.onnx"));
    expectException<std::runtime_error>([&] { nonfinite.synthesize("я"); });
    expectException<std::runtime_error>([&] { nonfinite.synthesize("ё"); });
    checkEmpty(nonfinite.synthesize("\t "));
    require(!nonfinite.synthesize("а").empty(),
            "Session must remain usable after a bad result");

    SileroTtsSynthesizer run_error(configFor(fixtures / "run_error.onnx"));
    expectException<std::runtime_error>([&] { run_error.synthesize("а"); });
    checkEmpty(run_error.synthesize("  "));
    require(!run_error.synthesize("дом").empty(),
            "Session must remain usable after an ONNX error");
}

void checkVocabularyAndLifetime(const fs::path &model, const fs::path &temp) {
    static_assert(!std::is_copy_constructible_v<SileroTtsSynthesizer>);
    static_assert(!std::is_copy_assignable_v<SileroTtsSynthesizer>);
    static_assert(std::is_nothrow_move_constructible_v<SileroTtsSynthesizer>);
    static_assert(std::is_nothrow_move_assignable_v<SileroTtsSynthesizer>);

    const auto tokens = temp / "tokens.txt";
    const auto copy = temp / "copy.onnx";
    fs::copy_file(model, copy);
    auto config = configFor(copy);
    config.tokens_path = tokens.string();
    for (const auto *invalid :
         {"~\n.\n \n", "~\n.\n \n+\n.\n", "~\n.\n \n+\nаа\n",
          "~\n.\n \n+\n\xff\n", "~\n.\n \n+\n\n"}) {
        writeFile(tokens, invalid);
        expectException<std::invalid_argument>(
            [&] { SileroTtsSynthesizer synthesizer(config); });
    }
    writeFile(tokens, CustomVocabulary);
    TtsAudio retained;
    {
        SileroTtsSynthesizer source(config);
        require(fs::remove(copy) && fs::remove(tokens),
                "Expected to remove the loaded files");
        SileroTtsSynthesizer moved(std::move(source));
        expectException<std::logic_error>([&] { source.synthesize("а"); });
        SileroTtsSynthesizer active(configFor(model));
        active = std::move(moved);
        require(active.getConfig().model_path == config.model_path &&
                    active.getConfig().tokens_path == config.tokens_path,
                "Configuration must follow the session on move");
        retained = active.synthesize("\tЁ+А\nQ 🙂__~ ");
        for (int i = 0; i < 1000; ++i) {
            checkEmpty(active.synthesize("123🙂"));
            checkEcho(active.synthesize("ё"), {2, 3, 1});
            checkEcho(active.synthesize("А А А А"),
                      {6, 5, 6, 5, 6, 5, 6, 3, 1});
        }
    }
    checkEcho(retained, {2, 4, 6, 5, 7, 3, 1});
}

} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected a TTS fixture directory");
        const fs::path fixtures = argv[1];
        const auto model = fixtures / "echo.onnx";
        smart_home::platform::TemporaryDirectory temp(fs::temp_directory_path(),
                                                      "smart-home-tts-");
        checkConfig(model, temp.path());
        checkText(model);
        checkOptions(model);
        checkModels(fixtures, temp.path());
        checkVocabularyAndLifetime(model, temp.path());
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
