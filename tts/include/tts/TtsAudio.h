/**
 * @file TtsAudio.h
 * @brief Результат синтеза монофонического PCM в оперативной памяти.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace smart_home::tts {

/** @brief Владеет двумя представлениями одного аудиофрагмента. */
struct TtsAudio {
    std::vector<float> samples_float; ///< Конечные отсчёты в диапазоне [-1, 1].
    std::vector<std::int16_t>
        samples_pcm16;      ///< Отсчёты в порядке байтов хоста.
    int sample_rate_hz = 0; ///< Частота в Гц, 0 для пустого результата.
    float duration_seconds =
        0.0f; ///< Число отсчётов, делённое на частоту, в секундах.

    /** @return true, если оба буфера пусты. */
    bool empty() const noexcept {
        return samples_float.empty() && samples_pcm16.empty();
    }
};

} // namespace smart_home::tts
