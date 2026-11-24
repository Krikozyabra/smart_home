/**
 * @file SherpaSttRecognizer.h
 * @brief Офлайн-распознаватель GigaAM-v3 на базе sherpa-onnx.
 */
#pragma once

#include "stt/ISttRecognizer.h"

#include <memory>

namespace smart_home::stt {

/**
 * @brief Владеет CPU-сессией и буфером нормализации PCM16.
 * @details Обрабатывает по одному фрагменту за раз. Для параллельного
 *          распознавания нужны независимые экземпляры.
 */
class SherpaSttRecognizer final : public ISttRecognizer {
  public:
    /**
     * @brief Проверяет файлы, загружает модель и прогревает её тишиной.
     * @param config Конфигурация. Файлы должны оставаться стабильными до
     *        завершения конструктора.
     * @throws std::invalid_argument Нарушение validateConfig().
     * @throws std::runtime_error Несовместимый SDK, модель, словарь или сбой
     * прогрева.
     */
    explicit SherpaSttRecognizer(SttConfig config);
    /** @brief Освобождает буфер и сессию без исключений. */
    ~SherpaSttRecognizer() override;

    SherpaSttRecognizer(const SherpaSttRecognizer &) = delete;
    SherpaSttRecognizer &operator=(const SherpaSttRecognizer &) = delete;
    /** @brief Передаёт владение созданной сессией другому экземпляру. */
    SherpaSttRecognizer(SherpaSttRecognizer &&) noexcept;
    /** @brief Освобождает прежнюю сессию и принимает новую. */
    SherpaSttRecognizer &operator=(SherpaSttRecognizer &&) noexcept;

    /** @return Конфигурация экземпляра. После передачи сессии настройки
     * следует читать у нового владельца.
     */
    const SttConfig &getConfig() const noexcept override;

    using ISttRecognizer::recognize;
    /** @copydoc ISttRecognizer::recognize(const std::int16_t*,std::size_t)
     * @throws std::logic_error Вызван перемещённый распознаватель с непустым
     * входом.
     */
    SttResult recognize(const std::int16_t *pcm_data,
                        std::size_t sample_count) override;

  private:
    struct Impl;
    SttConfig config_;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::stt
