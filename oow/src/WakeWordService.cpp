#include "oow/WakeWordService.h"
#include "logging/Logging.h"
#include "oow/OOWErrors.h"
#include "oow/UserProfileCalibrator.h"
#include <cmath>
#include <utility>

namespace smart_home::oow {
namespace {
// В отличие от thread::id, остаётся доступен после передачи thread на join.
thread_local WakeWordService *current_worker = nullptr;
} // namespace

WakeWordService::WakeWordService(std::unique_ptr<IAudioInput> audio_input,
                                 std::unique_ptr<IOOWDetector> detector)
    : audio_input_(std::move(audio_input)), detector_(std::move(detector)) {
    if (!audio_input_ || !detector_)
        throw std::invalid_argument(
            "Audio input and detector must not be null");
}

WakeWordService::~WakeWordService() noexcept { stopListening(); }

void WakeWordService::joinWorker(std::unique_lock<std::mutex> &lock) {
    lifecycle_changed_.wait(lock, [this] { return !joining_; });
    if (is_running_)
        return;
    if (!worker_thread_.joinable())
        return;
    joining_ = true;
    auto worker = std::move(worker_thread_);
    // Callback может вызвать stopListening, поэтому нельзя держать
    // lifecycle_mutex_.
    lock.unlock();
    worker.join();
    lock.lock();
    joining_ = false;
    lifecycle_changed_.notify_all();
}

void WakeWordService::startListening() {
    std::unique_lock<std::mutex> lock(lifecycle_mutex_);
    if (is_running_)
        return;
    if (current_worker == this)
        throw std::logic_error(
            "Cannot restart the service from a stopping callback");
    joinWorker(lock);
    // Пока ожидали чужой join, другой start мог уже запустить поток.
    if (is_running_)
        return;
    {
        std::lock_guard<std::mutex> detector_lock(detector_mutex_);
        detector_->reset();
    }
    try {
        // stop нужен и при частично неудавшемся open/start.
        audio_open_ = true;
        audio_input_->open(AudioFormat{});
        if (!audio_input_->isAvailable())
            throw MicrophoneNotFoundException("устройство недоступно");
        audio_input_->start();
    } catch (const MicrophoneNotFoundException &error) {
        logging::log(logging::Level::Error, "OOW", error.what());
        stopAudioLocked();
        throw;
    } catch (const std::exception &error) {
        MicrophoneNotFoundException wrapped(error.what());
        logging::log(logging::Level::Error, "OOW", wrapped.what());
        stopAudioLocked();
        throw wrapped;
    } catch (...) {
        stopAudioLocked();
        MicrophoneNotFoundException error("неизвестная ошибка аудиовхода");
        logging::log(logging::Level::Error, "OOW", error.what());
        throw error;
    }
    intent_buffer_.reset();
    state_ = State::Listening;
    is_running_ = true;
    try {
        worker_thread_ = std::thread(&WakeWordService::workerLoop, this);
    } catch (...) {
        is_running_ = false;
        state_ = State::Idle;
        stopAudioLocked();
        throw;
    }
}

void WakeWordService::stopAudioLocked() noexcept {
    if (!audio_open_)
        return;
    audio_open_ = false;
    try {
        audio_input_->stop();
    } catch (const std::exception &error) {
        logging::log(logging::Level::Error, "OOW", error.what());
    } catch (...) {
        logging::log(logging::Level::Error, "OOW",
                     "Не удалось остановить микрофон");
    }
}

void WakeWordService::stopListening() {
    std::unique_lock<std::mutex> lock(lifecycle_mutex_);
    is_running_ = false;
    stopAudioLocked();
    if (current_worker == this)
        return;
    joinWorker(lock);
}

void WakeWordService::calibrateUserProfile(
    const std::vector<AudioClip> &samples) {
    std::lock_guard<std::mutex> lock(detector_mutex_);
    detector_->reset();
    try {
        UserProfileCalibrator(*detector_).calibrate(samples);
    } catch (...) {
        // Ошибка очистки не должна скрыть исходную ошибку проверки записей.
        try {
            detector_->reset();
        } catch (...) {
        }
        throw;
    }
    detector_->reset();
}

void WakeWordService::setWakeWordCallback(WakeWordCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    wake_cb_ = std::move(cb);
}

void WakeWordService::setIntentAudioCallback(IntentAudioCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    intent_cb_ = std::move(cb);
}

bool WakeWordService::isListening() const noexcept {
    return is_running_.load();
}

void WakeWordService::workerLoop() noexcept {
    current_worker = this;
    try {
        std::vector<int16_t> chunk;
        chunk.reserve(kChunkSizeSamples);
        while (is_running_) {
            const auto read = audio_input_->readChunk(chunk, kChunkSizeSamples);
            if (!is_running_)
                break;
            if (!read)
                throw MicrophoneNotFoundException(
                    "поток завершён или произошла ошибка чтения");
            if (chunk.empty() || chunk.size() > kChunkSizeSamples)
                throw MicrophoneNotFoundException(
                    "некорректный размер аудиочанка");
            if (state_ == State::Listening) {
                DetectionResult result;
                {
                    std::lock_guard<std::mutex> lock(detector_mutex_);
                    result = detector_->feed(chunk.data(), chunk.size());
                }
                if (!std::isfinite(result.confidence) ||
                    result.confidence < 0 || result.confidence > 1)
                    throw OOWException(
                        "Некорректная уверенность потокового детектора");
                if (result.confidence < kActivationThreshold)
                    continue;
                state_ = State::RecordingIntent;
                intent_buffer_.reset();
                logging::log(logging::Level::Info, "OOW",
                             "Запись пошла: обнаружено ключевое слово "
                             "'Джарвис' (уверенность: " +
                                 std::to_string(result.confidence) + ")");
                WakeWordCallback cb;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    cb = wake_cb_;
                }
                if (cb)
                    cb(result.confidence);
                if (!is_running_)
                    break;
            }
            intent_buffer_.pushChunk(chunk.data(), chunk.size());
            if (intent_buffer_.isComplete()) {
                IntentAudioCallback cb;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    cb = intent_cb_;
                }
                if (cb)
                    cb(intent_buffer_.data());
                {
                    std::lock_guard<std::mutex> lock(detector_mutex_);
                    detector_->reset();
                }
                state_ = State::Listening;
            }
        }
    } catch (const MissingKeywordException &) {
        // Калибровка, вызванная из callback, уже записала эту ошибку в лог.
    } catch (const std::exception &error) {
        logging::log(logging::Level::Error, "OOW", error.what());
    } catch (...) {
        logging::log(logging::Level::Error, "OOW",
                     "Неизвестная ошибка фонового прослушивания");
    }
    state_ = State::Idle;
    intent_buffer_.reset();
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        is_running_ = false;
        stopAudioLocked();
    }
    current_worker = nullptr;
}

} // namespace smart_home::oow
