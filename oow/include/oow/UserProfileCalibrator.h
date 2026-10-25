#pragma once

#include "oow/interfaces/IOOWDetector.h"

namespace smart_home::oow {

/** @brief Проверка качества пяти записей; веса модели и файлы не изменяются.
 * @details Детектор заимствуется и должен жить дольше калибратора.
 * Вызывающая сторона обеспечивает исключительный доступ к детектору.
 */
class UserProfileCalibrator {
  public:
    explicit UserProfileCalibrator(IOOWDetector &detector)
        : detector_(detector) {}
    /** @throws std::invalid_argument Неверное число, формат или пустая запись.
     * @throws MissingKeywordException Уверенность одной записи ниже 0.65.
     * @throws OOWException Детектор вернул некорректную уверенность.
     */
    void calibrate(const std::vector<AudioClip> &samples);

  private:
    IOOWDetector &detector_;
};

} // namespace smart_home::oow
