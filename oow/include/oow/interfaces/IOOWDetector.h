#pragma once

#include "oow/AudioTypes.h"

namespace smart_home::oow {

/** @brief Детектор слова «Джарвис»; уверенность — конечное число в [0, 1].
 * @details Сервис сериализует обращения. Указатели аудио действуют только
 * до возврата вызова. processClip возвращает пиковую уверенность всей записи.
 */
class IOOWDetector {
  public:
    virtual ~IOOWDetector() = default;
    virtual DetectionResult feed(const int16_t *samples, size_t count) = 0;
    virtual float processClip(const AudioClip &clip) = 0;
    virtual void reset() = 0;
};

} // namespace smart_home::oow
