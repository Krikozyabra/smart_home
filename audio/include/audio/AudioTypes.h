/** @file AudioTypes.h
 * @brief Собственные копии PCM и метаданные последовательности захвата.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace smart_home::audio {

/** @brief Signed PCM16 в порядке байтов хоста; Stereo чередует L, R. */
using PcmSamples = std::vector<std::int16_t>;

/** @brief Полный кадр одного сеанса; samples владеет данными. */
struct AudioFrame {
    std::uint64_t sequence =
        0;              ///< Номер с нуля; пропуск означает потерю кадров.
    PcmSamples samples; ///< Отсчёты всех каналов interleaved.
};

/** @brief Независимая согласованная копия кольца от старых кадров к новым. */
struct AudioSnapshot {
    std::uint64_t session_id = 0; ///< Ноль до запуска, новый номер при restart.
    std::vector<AudioFrame> frames; ///< Чтение не удаляет кадры из кольца.
};

} // namespace smart_home::audio
