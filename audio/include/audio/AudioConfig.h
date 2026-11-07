/** @file AudioConfig.h
 * @brief Неизменяемые после создания захвата параметры PCM и кольца.
 */
#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace smart_home::audio {

/** @brief Число чередующихся каналов PCM. */
enum class Channels { Mono = 1, Stereo = 2 };

/** @brief Настройки одного capture PCM; значения длительностей в миллисекундах.
 */
struct AudioConfig {
    std::string device_name = "default"; ///< Имя PCM ALSA, непустое и без NUL.
    int sample_rate_hz = 16000;          ///< Положительная точная частота, Гц.
    Channels channels = Channels::Mono;  ///< Mono или Stereo (L, R).
    std::chrono::milliseconds frame_duration{
        80};                                  ///< Длительность полного кадра.
    std::size_t buffer_capacity_frames = 125; ///< Положительная ёмкость кольца.
};

/** @brief Проверяет все параметры и размеры без открытия PCM и выделения
 * кольца.
 * @throws std::invalid_argument Некорректные параметры, дробное число отсчётов
 * или переполнение размеров контейнеров.
 */
void validateConfig(const AudioConfig &config);

} // namespace smart_home::audio
