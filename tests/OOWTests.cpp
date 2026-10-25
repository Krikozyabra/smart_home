#include "TestUtil.h"
#include "Testing.h"
#include "logging/Logging.h"
#include "oow/MockAudioInput.h"
#include "oow/MockOOWDetector.h"
#include "oow/UserProfileCalibrator.h"
#include "oow/WakeWordService.h"
#include <spdlog/sinks/ostream_sink.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <sstream>

using namespace smart_home::oow;
using namespace std::chrono_literals;
namespace logging = smart_home::logging;

namespace {
struct Fixture {
    MockAudioInput *audio;
    MockOOWDetector *detector;
    WakeWordService service;
    Fixture()
        : Fixture(std::make_unique<MockAudioInput>(),
                  std::make_unique<MockOOWDetector>()) {}
    Fixture(std::unique_ptr<MockAudioInput> input,
            std::unique_ptr<MockOOWDetector> engine)
        : audio(input.get()), detector(engine.get()),
          service(std::move(input), std::move(engine)) {}
};

std::vector<AudioClip> clips() {
    return std::vector<AudioClip>(
        5, AudioClip{std::vector<int16_t>(1280, 42), {}});
}

size_t occurrences(const std::string &text, const std::string &word) {
    size_t count = 0, pos = 0;
    while ((pos = text.find(word, pos)) != std::string::npos) {
        ++count;
        pos += word.size();
    }
    return count;
}

struct Logs {
    std::ostringstream output;
    Logs() {
        logging::Config config;
        config.file_path.clear();
        logging::initialize(config);
        logging::installTestSink(
            std::make_shared<spdlog::sinks::ostream_sink_mt>(output));
    }
    ~Logs() { logging::shutdown(); }
};

template <class T> void ready(std::future<T> &result) {
    require(result.wait_for(2s) == std::future_status::ready,
            "Asynchronous operation timed out");
}

void testMicrophoneNotFound() {
    for (int failure = 0; failure < 3; ++failure) {
        Logs logs;
        Fixture f;
        f.audio->simulate_fail_open = failure == 0;
        f.audio->simulate_unavailable = failure == 1;
        f.audio->simulate_fail_start = failure == 2;
        expectException<MicrophoneNotFoundException>(
            [&] { f.service.startListening(); });
        require(!f.service.isListening() && f.audio->stops() == 1,
                "Failed startup cleans up input");
        require(occurrences(logs.output.str(),
                            "Микрофон не найден или не настроен") == 1,
                "Microphone startup failure must be logged exactly once");
        f.audio->simulate_fail_open = f.audio->simulate_unavailable =
            f.audio->simulate_fail_start = false;
        f.service.startListening();
        require(f.audio->waitReads(1), "Startup must recover after failure");
        require(f.audio->opened_format.sample_rate == 16000 &&
                    f.audio->opened_format.channels == 1 &&
                    f.audio->opened_format.bits_per_sample == 16,
                "Input must open in the required PCM format");
        f.service.stopListening();
    }
    expectException<std::invalid_argument>([] {
        WakeWordService service(nullptr, std::make_unique<MockOOWDetector>());
    });
    expectException<std::invalid_argument>([] {
        WakeWordService service(std::make_unique<MockAudioInput>(), nullptr);
    });
}

void testCalibrationValidation() {
    Logs logs;
    MockOOWDetector detector;
    UserProfileCalibrator calibrator(detector);
    for (size_t count : {0u, 4u, 6u}) {
        auto samples = clips();
        samples.resize(count);
        expectException<std::invalid_argument>(
            [&] { calibrator.calibrate(samples); });
    }
    require(detector.clips == 0,
            "Wrong count must be rejected before inference");
    for (int invalid = 0; invalid < 4; ++invalid) {
        auto samples = clips();
        if (invalid == 0)
            samples[4].samples.clear();
        if (invalid == 1)
            samples[4].format.sample_rate = 8000;
        if (invalid == 2)
            samples[4].format.channels = 2;
        if (invalid == 3)
            samples[4].format.bits_per_sample = 8;
        expectException<std::invalid_argument>(
            [&] { calibrator.calibrate(samples); });
    }
    require(detector.clips == 0,
            "All clip formats must be checked before inference");
    for (size_t index = 0; index < 5; ++index) {
        detector.clips = 0;
        detector.clip_confidences.assign(5, 0.65f);
        detector.clip_confidences[index] = 0.40f;
        bool caught = false;
        try {
            calibrator.calibrate(clips());
        } catch (const MissingKeywordException &error) {
            caught = true;
            require(error.sampleIndex() == index && error.confidence() == 0.40f,
                    "Missing keyword exception must identify the clip and "
                    "confidence");
        }
        require(caught && detector.clips == index + 1,
                "Calibration must fail fast at the bad clip");
    }
    require(occurrences(logs.output.str(), "нет ключевого слова") == 5,
            "One error log per bad clip");
    detector.clips = 0;
    detector.clip_confidences.assign(5, 0.65f);
    calibrator.calibrate(clips());
    require(detector.clips == 5 &&
                logs.output.str().find("успешно завершена по 5 записям") !=
                    std::string::npos,
            "Inclusive calibration threshold and success log");
    for (float invalid :
         {std::numeric_limits<float>::quiet_NaN(), -0.1f, 1.1f}) {
        detector.clips = 0;
        detector.clip_confidences[0] = invalid;
        expectException<OOWException>([&] { calibrator.calibrate(clips()); });
    }
    Fixture f;
    f.service.calibrateUserProfile(clips());
    require(f.detector->clips == 5 && f.detector->resets == 2,
            "Service delegates calibration and resets context");
    f.detector->clips = 0;
    f.detector->clip_confidences = {0.4f};
    expectException<MissingKeywordException>(
        [&] { f.service.calibrateUserProfile(clips()); });
    require(f.detector->resets == 4, "Failed calibration also resets context");
}

void testIntentBuffer() {
    IntentCaptureBuffer buffer;
    buffer.pushChunk(nullptr, 0);
    expectException<std::invalid_argument>(
        [&] { buffer.pushChunk(nullptr, 1); });
    std::vector<int16_t> samples(kIntentAudioSamplesCount + 17, 123);
    buffer.pushChunk(samples.data(), 31);
    buffer.pushChunk(samples.data() + 31, samples.size() - 31);
    require(buffer.isComplete() && buffer.data().size() == 160000 &&
                std::all_of(buffer.data().begin(), buffer.data().end(),
                            [](auto v) { return v == 123; }),
            "Buffer must preserve samples and truncate excess without "
            "exceeding 10 seconds");
    buffer.pushChunk(samples.data(), samples.size());
    require(buffer.data().size() == 160000, "Complete buffer remains bounded");
    buffer.reset();
    require(!buffer.isComplete() && buffer.data().empty(),
            "Reset must clear the recording");
}

void testKeywordDetectionAndIntentCapture() {
    Logs logs;
    Fixture f;
    f.detector->feed_confidences = {0.79f, 0.80f, 0.85f};
    std::atomic<int> wakes{0};
    std::vector<std::vector<int16_t>> recordings;
    std::promise<void> complete;
    auto done = complete.get_future();
    f.service.setWakeWordCallback([&](float confidence) {
        require(confidence >= 0.80f && f.service.isListening(),
                "Inclusive activation threshold");
        ++wakes;
        // Повторная регистрация и калибровка проверяют отсутствие блокировок в
        // callback.
        f.service.setWakeWordCallback({});
        f.service.calibrateUserProfile(clips());
    });
    f.service.setIntentAudioCallback([&](const auto &samples) {
        recordings.push_back(samples);
        f.service.setWakeWordCallback([&](float confidence) {
            require(confidence == 0.85f, "Second activation confidence");
            ++wakes;
        });
        if (recordings.size() == 2) {
            f.service.stopListening();
            complete.set_value();
        }
    });
    // 1 чанк ниже порога и два независимых захвата по 125 чанков.
    for (int chunk = 0; chunk < 251; ++chunk)
        f.audio->push(std::vector<int16_t>(1280, static_cast<int16_t>(chunk)));
    f.service.startListening();
    ready(done);
    done.get();
    f.service.stopListening();
    require(wakes == 2 && recordings.size() == 2,
            "Exactly two activations and recordings");
    for (size_t capture = 0; capture < recordings.size(); ++capture) {
        const auto &samples = recordings[capture];
        require(samples.size() == 160000,
                "Intent callback must receive exactly 10 seconds");
        for (size_t i = 0; i < samples.size(); ++i)
            require(samples[i] ==
                        static_cast<int16_t>(1 + capture * 125 + i / 1280),
                    "Recording must include the trigger chunk and preserve PCM "
                    "order");
    }
    require(f.detector->feeds == 3 && f.detector->resets == 5,
            "Detection pauses during capture, resets after each capture and "
            "calibration");
    require(occurrences(logs.output.str(),
                        "Запись пошла: обнаружено ключевое слово 'Джарвис'") ==
                2,
            "One activation log per recording");
}

void testConcurrentStop() {
    Fixture f;
    for (size_t run = 1; run <= 3; ++run) {
        f.service.startListening();
        f.service.startListening();
        require(f.audio->waitReads(run), "Worker must reach blocking read");
        auto first =
            std::async(std::launch::async, [&] { f.service.stopListening(); });
        auto second =
            std::async(std::launch::async, [&] { f.service.stopListening(); });
        ready(first);
        ready(second);
        first.get();
        second.get();
        require(!f.service.isListening() && f.audio->stops() == run,
                "Concurrent stop joins and releases input once");
    }
    // Деструктор должен разбудить заблокированный readChunk.
    {
        Fixture active;
        active.service.startListening();
        require(active.audio->waitReads(1), "Destructor test reaches read");
    }
}

void testCallbackStopDuringExternalJoin() {
    Fixture f;
    f.detector->feed_confidences = {0.9f};
    f.audio->push(std::vector<int16_t>(1280, 1));
    std::promise<void> entered, release;
    auto callback_entered = entered.get_future();
    auto callback_release = release.get_future();
    f.service.setWakeWordCallback([&](float) {
        entered.set_value();
        require(callback_release.wait_for(2s) == std::future_status::ready,
                "Release callback for shutdown");
        f.service.stopListening();
        expectException<std::logic_error>([&] { f.service.startListening(); });
        f.service.setIntentAudioCallback({});
    });
    f.service.startListening();
    ready(callback_entered);
    auto stopping =
        std::async(std::launch::async, [&] { f.service.stopListening(); });
    // Поток удерживается callback; внешняя остановка уже может ожидать join.
    require(f.audio->waitStops(1),
            "External stop must release the input before joining");
    release.set_value();
    ready(stopping);
    stopping.get();
    require(!f.service.isListening(),
            "Callback stop and external join must not deadlock");
}

void testWorkerFailures() {
    for (int failure = 0; failure < 7; ++failure) {
        Logs logs;
        Fixture f;
        std::promise<void> callback_entered;
        auto callback = callback_entered.get_future();
        if (failure == 0)
            f.audio->simulate_fail_read = true;
        if (failure == 1)
            f.audio->simulate_eof = true;
        if (failure == 2)
            f.audio->push({});
        if (failure == 3) {
            f.detector->fail_feed = true;
            f.audio->push({1});
        }
        if (failure == 4) {
            f.detector->feed_confidences = {
                std::numeric_limits<float>::quiet_NaN()};
            f.audio->push({1});
        }
        if (failure >= 5) {
            f.detector->feed_confidences = {0.9f};
            f.service.setWakeWordCallback([&](float) {
                callback_entered.set_value();
                throw std::runtime_error("mock callback failure");
            });
            if (failure == 6)
                f.audio->simulate_fail_stop = true;
            f.audio->push({1});
        }
        f.service.startListening();
        require(f.audio->waitReads(1), "Failure test worker must run");
        if (failure >= 5) {
            ready(callback);
            callback.get();
        }
        require(f.audio->waitStops(1),
                "Worker failure must stop the input automatically");
        require(!f.service.isListening(),
                "Worker failure must end listening without terminate");
        f.service.stopListening();
        require(f.audio->stops() == 1,
                "Worker failure must release input once");
        require(occurrences(logs.output.str(), "[error]") ==
                    (failure == 6 ? 2u : 1u),
                "Each worker/cleanup failure must be logged once");
    }
}

void testCalibrationWhileListening() {
    Fixture f;
    std::promise<void> analyzing, release;
    auto entered = analyzing.get_future();
    auto released = release.get_future().share();
    f.detector->on_clip = [&] {
        if (f.detector->clips == 1) {
            analyzing.set_value();
            require(released.wait_for(2s) == std::future_status::ready,
                    "Calibration release timeout");
        }
    };
    f.service.startListening();
    require(f.audio->waitReads(1), "Worker must wait for audio");
    auto calibration = std::async(
        std::launch::async, [&] { f.service.calibrateUserProfile(clips()); });
    ready(entered);
    f.audio->push({1});
    require(f.detector->feeds == 0, "Inference must not overlap calibration");
    release.set_value();
    ready(calibration);
    calibration.get();
    require(f.audio->waitReads(2), "Inference resumes after calibration");
    f.service.stopListening();
    require(f.detector->feeds == 1 && f.detector->clips == 5,
            "Both calibration and inference complete");
}

void testCalibrationFailureInCallback() {
    Logs logs;
    Fixture f;
    f.detector->feed_confidences = {0.9f};
    f.detector->clip_confidences = {0.4f};
    f.audio->push({1});
    f.service.setWakeWordCallback(
        [&](float) { f.service.calibrateUserProfile(clips()); });
    f.service.startListening();
    require(f.audio->waitStops(1),
            "Failed callback calibration must stop the worker");
    f.service.stopListening();
    require(
        occurrences(logs.output.str(), "нет ключевого слова") == 1,
        "Callback calibration failure must not be logged again by the worker");
}

void testConcurrentStartStop() {
    Fixture f;
    auto starting = std::async(std::launch::async, [&] {
        for (int i = 0; i < 50; ++i)
            f.service.startListening();
    });
    auto stopping = std::async(std::launch::async, [&] {
        for (int i = 0; i < 50; ++i)
            f.service.stopListening();
    });
    auto restarting = std::async(std::launch::async, [&] {
        for (int i = 0; i < 50; ++i) {
            f.service.stopListening();
            f.service.startListening();
        }
    });
    ready(starting);
    ready(stopping);
    ready(restarting);
    starting.get();
    stopping.get();
    restarting.get();
    f.service.stopListening();
    require(!f.service.isListening(),
            "Concurrent lifecycle operations must finish cleanly");
}
} // namespace

int main() {
    try {
        testMicrophoneNotFound();
        testCalibrationValidation();
        testIntentBuffer();
        testKeywordDetectionAndIntentCapture();
        testConcurrentStop();
        testCallbackStopDuringExternalJoin();
        testWorkerFailures();
        testCalibrationWhileListening();
        testCalibrationFailureInCallback();
        testConcurrentStartStop();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
