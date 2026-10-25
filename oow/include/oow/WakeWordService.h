#pragma once

#include "oow/IntentCaptureBuffer.h"
#include "oow/interfaces/IAudioInput.h"
#include "oow/interfaces/IOOWDetector.h"
#include "oow/interfaces/IWakeWordService.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace smart_home::oow {

/** @brief Владеет источником, детектором и рабочим потоком.
 * @details Методы допускают конкурентные вызовы. startListening идемпотентен
 * при работе; перезапуск из останавливающегося callback запрещён (logic_error).
 * stopListening из callback запрашивает остановку без self-join; внешний вызов
 * ждёт завершения потока. Уничтожать сервис внутри callback нельзя, владелец
 * обязан пережить callback. Исключения рабочего потока логируются и
 * останавливают сервис; синхронные ошибки запуска микрофона логируются и
 * пробрасываются.
 */
class WakeWordService final : public IWakeWordService {
  public:
    WakeWordService(std::unique_ptr<IAudioInput> audio_input,
                    std::unique_ptr<IOOWDetector> detector);
    ~WakeWordService() noexcept override;

    void startListening() override;
    void stopListening() override;
    /** @details Сериализует анализ с потоковым инференсом и сбрасывает
     * контекст. Инференс ждёт освобождения детектора; возможен один уже
     * прочитанный чанк.
     */
    void calibrateUserProfile(const std::vector<AudioClip> &samples) override;
    void setWakeWordCallback(WakeWordCallback cb) override;
    void setIntentAudioCallback(IntentAudioCallback cb) override;
    bool isListening() const noexcept override;

  private:
    enum class State { Idle, Listening, RecordingIntent };
    void workerLoop() noexcept;
    void stopAudioLocked() noexcept;
    void joinWorker(std::unique_lock<std::mutex> &lock);

    std::unique_ptr<IAudioInput> audio_input_;
    std::unique_ptr<IOOWDetector> detector_;
    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;
    std::mutex lifecycle_mutex_;
    std::condition_variable lifecycle_changed_;
    bool joining_{false};
    bool audio_open_{false};
    std::mutex detector_mutex_;
    std::mutex mutex_;
    WakeWordCallback wake_cb_;
    IntentAudioCallback intent_cb_;
    // После запуска состоянием и буфером владеет только рабочий поток.
    State state_{State::Idle};
    IntentCaptureBuffer intent_buffer_;
};

} // namespace smart_home::oow
