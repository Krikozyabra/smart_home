/** @file OpenWakeWordDetector.h
 * @brief Детектор одного ключевого слова на CPU с помощью ONNX Runtime.
 */
#pragma once

#include "oww/IOpenWakeWord.h"
#include "oww/OwwConfig.h"

#include <memory>

namespace smart_home::oww {

/** @brief Загружает три модели OpenWakeWord один раз и сохраняет контекст
 * потока.
 * @details Публичный интерфейс совместим с C++17 и не раскрывает ONNX Runtime.
 * Аудиобуферы и тензоры создаются и прогреваются в конструкторе.
 */
class OpenWakeWordDetector final : public IOpenWakeWord {
  public:
    /** @brief Проверяет файлы, загружает модели и прогревает пайплайн тишиной.
     * @param config Пути и ограничения ресурсов.
     * @throws std::invalid_argument Некорректные настройки или несовместимый
     * интерфейс модели (ожидается бинарный классификатор OpenWakeWord).
     * @throws std::runtime_error Не удалось загрузить или выполнить
     * ONNX-модель.
     */
    explicit OpenWakeWordDetector(OwwConfig config);
    ~OpenWakeWordDetector() override; ///< Освобождает сессии и рабочую память.

    OpenWakeWordDetector(const OpenWakeWordDetector &) =
        delete; ///< Контекст не копируется.
    OpenWakeWordDetector &operator=(const OpenWakeWordDetector &) =
        delete; ///< Контекст не копируется.

    using IOpenWakeWord::processFrame;
    const std::string &getModelPath() const noexcept override;
    float processFrame(const std::int16_t *pcm_frame) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::oww
