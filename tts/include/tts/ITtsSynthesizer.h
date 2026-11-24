/**
 * @file ITtsSynthesizer.h
 * @brief Синхронный контракт текстового синтеза речи.
 */
#pragma once

#include "tts/TtsAudio.h"
#include "tts/TtsConfig.h"

#include <stdexcept>
#include <string>
#include <string_view>

namespace smart_home::tts {

/**
 * @brief Преобразует независимые русскоязычные фрагменты текста в моно PCM.
 * @details Обращения к одному экземпляру сериализует вызывающий код.
 */
class ITtsSynthesizer {
  public:
    /** @brief Освобождает ресурсы синтезатора. */
    virtual ~ITtsSynthesizer() = default;

    /** @return Ссылка на настройки. Они принадлежат экземпляру. */
    virtual const TtsConfig &getConfig() const noexcept = 0;

    /**
     * @brief Синтезирует речь из текста с ручной разметкой ударений.
     * @details Ставьте `+` перед ударной русской гласной.
     * @param text Строка UTF-8, действительная до возврата из вызова.
     * @return Собственные буферы PCM. Текст без токенов даёт TtsAudio{}.
     * @throws std::invalid_argument Некорректная кодировка UTF-8.
     * @throws std::runtime_error Ошибка инференса или результата модели.
     */
    virtual TtsAudio synthesize(std::string_view text) = 0;

    /** @copydoc synthesize(std::string_view) */
    TtsAudio synthesize(const std::string &text) {
        return synthesize(std::string_view(text));
    }

    /**
     * @copydoc synthesize(std::string_view)
     * @throws std::invalid_argument Указатель text равен nullptr.
     * @pre Строка по указателю text заканчивается нулевым байтом.
     */
    TtsAudio synthesize(const char *text) {
        if (text == nullptr)
            throw std::invalid_argument("TTS text must not be null.");
        return synthesize(std::string_view(text));
    }
};

} // namespace smart_home::tts
