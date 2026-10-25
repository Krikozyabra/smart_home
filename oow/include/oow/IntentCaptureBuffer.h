#pragma once

#include "oow/AudioTypes.h"
#include <algorithm>
#include <stdexcept>

namespace smart_home::oow {

/** @brief Ограниченный RAM-буфер; лишний хвост чанка отбрасывается.
 * @details Владелец сериализует доступ. data() действует до
 * изменения/уничтожения буфера. reset сохраняет выделенную память для следующей
 * записи.
 */
class IntentCaptureBuffer {
  public:
    IntentCaptureBuffer() { samples_.reserve(kIntentAudioSamplesCount); }

    void pushChunk(const int16_t *data, size_t count) {
        if (count != 0 && data == nullptr)
            throw std::invalid_argument("Null audio samples");
        const auto accepted =
            std::min(count, kIntentAudioSamplesCount - samples_.size());
        if (accepted != 0)
            samples_.insert(samples_.end(), data, data + accepted);
    }

    bool isComplete() const {
        return samples_.size() == kIntentAudioSamplesCount;
    }
    const std::vector<int16_t> &data() const { return samples_; }
    void reset() { samples_.clear(); }

  private:
    std::vector<int16_t> samples_;
};

} // namespace smart_home::oow
