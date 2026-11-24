/**
 * @file ISttRecognizer.h
 * @brief Синхронное распознавание готового монофонического аудиофрагмента.
 */
#pragma once

#include "stt/SttConfig.h"
#include "stt/SttResult.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace smart_home::stt {

/**
 * @brief Интерфейс распознавания последовательных независимых фрагментов.
 * @details Одновременные вызовы recognize() для одного экземпляра
 *          синхронизирует вызывающий код. Каждый вызов удерживает вход только
 *          до возврата.
 */
class ISttRecognizer {
  public:
    /** @brief Освобождает ресурсы распознавателя. */
    virtual ~ISttRecognizer() = default;

    /** @return Конфигурация, ссылка действительна до уничтожения экземпляра. */
    virtual const SttConfig &getConfig() const noexcept = 0;

    /**
     * @brief Распознаёт моно PCM16 с частотой 16000 Гц в порядке байтов хоста.
     * @param pcm_data Заимствованный буфер минимум из sample_count отсчётов.
     * @param sample_count Число отсчётов, максимум INT32_MAX.
     * @return Результат распознавания. Для nullptr или нуля отсчётов
     * возвращается SttResult{}.
     * @throws std::invalid_argument Слишком длинный ненулевой буфер.
     * @throws std::runtime_error Сбой инференса или некорректный результат SDK.
     */
    virtual SttResult recognize(const std::int16_t *pcm_data,
                                std::size_t sample_count) = 0;

    /**
     * @brief Передаёт отсчёты вектора в recognize() без копирования.
     * @param pcm_data Моно PCM16 при 16000 Гц.
     * @return Результат распознавания фрагмента.
     */
    SttResult recognize(const std::vector<std::int16_t> &pcm_data) {
        return recognize(pcm_data.data(), pcm_data.size());
    }
};

} // namespace smart_home::stt
