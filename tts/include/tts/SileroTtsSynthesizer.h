/**
 * @file SileroTtsSynthesizer.h
 * @brief Офлайн-синтезатор Silero TTS на CPU.
 */
#pragma once

#include "tts/ITtsSynthesizer.h"

#include <memory>

namespace smart_home::tts {

/**
 * @brief Владеет сессией ONNX Runtime, словарём и рабочими буферами.
 * @details Выполняйте вызовы одного экземпляра последовательно. Для
 * параллельного синтеза используйте независимые экземпляры.
 */
class SileroTtsSynthesizer final : public ITtsSynthesizer {
  public:
    /**
     * @brief Проверяет параметры, загружает словарь и модель, прогревает
     * сессию.
     * @param config Настройки. Файлы должны оставаться стабильными до возврата.
     * @throws std::invalid_argument Нарушение validateConfig() или формата
     * словаря.
     * @throws std::runtime_error Несовместимый ONNX-граф или сбой прогрева.
     */
    explicit SileroTtsSynthesizer(TtsConfig config);
    /** @brief Освобождает ресурсы сессии без исключений. */
    ~SileroTtsSynthesizer() override;

    SileroTtsSynthesizer(const SileroTtsSynthesizer &) = delete;
    SileroTtsSynthesizer &operator=(const SileroTtsSynthesizer &) = delete;
    /** @brief Передаёт владение сессией. */
    SileroTtsSynthesizer(SileroTtsSynthesizer &&) noexcept;
    /** @brief Освобождает прежнюю сессию и принимает новую. */
    SileroTtsSynthesizer &operator=(SileroTtsSynthesizer &&) noexcept;

    /**
     * @return Конфигурация. Ссылка действительна до уничтожения экземпляра.
     * @details После перемещения читайте настройки у владельца сессии.
     */
    const TtsConfig &getConfig() const noexcept override;

    using ITtsSynthesizer::synthesize;
    /**
     * @copydoc ITtsSynthesizer::synthesize(std::string_view)
     * @throws std::logic_error Экземпляр передал сессию при перемещении.
     */
    TtsAudio synthesize(std::string_view text) override;

  private:
    struct Impl;
    TtsConfig config_;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::tts
