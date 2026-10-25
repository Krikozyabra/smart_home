#pragma once

#include "oow/AudioTypes.h"

namespace smart_home::oow {

/** @brief Источник PCM без записи аудио на диск.
 * @details readChunk вызывается рабочим потоком; stop должен быть безопасен
 * одновременно с readChunk и разблокировать его, в том числе при ошибке
 * остановки. Ошибки устройства выбрасываются как MicrophoneNotFoundException.
 * Реализация не логирует ошибки: граница обработки находится в сервисе.
 */
class IAudioInput {
  public:
    virtual ~IAudioInput() = default;
    virtual void open(const AudioFormat &format) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    /** @return false при завершении/сбое; true требует 1..chunk_size сэмплов.
     * @details Вызов блокирующий с конечным таймаутом, без активного ожидания.
     */
    virtual bool readChunk(std::vector<int16_t> &out_chunk,
                           size_t chunk_size) = 0;
    virtual bool isAvailable() const noexcept = 0;
};

} // namespace smart_home::oow
