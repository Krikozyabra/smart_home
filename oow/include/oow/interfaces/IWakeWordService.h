#pragma once

#include "oow/AudioTypes.h"
#include <functional>

namespace smart_home::oow {

using WakeWordCallback = std::function<void(float confidence)>;
using IntentAudioCallback =
    std::function<void(const std::vector<int16_t> &audio_10s)>;

/** @brief Управление захватом и проверкой пяти записей.
 * @details Callbacks синхронно выполняются в рабочем потоке без блокировок
 * сервиса. Ссылка на аудио действительна до возврата callback; для хранения
 * нужна копия. Медленный callback задерживает захват.
 */
class IWakeWordService {
  public:
    virtual ~IWakeWordService() = default;
    virtual void startListening() = 0;
    virtual void stopListening() = 0;
    virtual void
    calibrateUserProfile(const std::vector<AudioClip> &samples) = 0;
    virtual void setWakeWordCallback(WakeWordCallback cb) = 0;
    virtual void setIntentAudioCallback(IntentAudioCallback cb) = 0;
    virtual bool isListening() const noexcept = 0;
};

} // namespace smart_home::oow
