/**
 * @file TtsConfig.h
 * @brief Настройки офлайн-синтеза речи.
 */
#pragma once

#include <string>

namespace smart_home::tts {

/** @brief Пути модели и словаря, формат выходного PCM и параметры CPU. */
struct TtsConfig {
    std::string model_path; ///< Путь к читаемому ONNX-файлу Silero TTS.
    /** @brief Словарь UTF-8 с одним символом на строку в порядке индексов.
     * @details Пустой путь выбирает встроенный словарь.
     */
    std::string tokens_path;
    int sample_rate_hz = 24000; ///< Выходная частота 8000, 24000 или 48000 Гц.
    /** @brief aidar, baya, kseniya, xenia, eugene или random. */
    std::string speaker = "xenia";
    int num_threads = 2; ///< Число потоков CPU инференса, от 1 до 4.
};

/**
 * @brief Проверяет значения и доступность файлов перед загрузкой модели.
 * @param config Параметры синтезатора.
 * @throws std::invalid_argument Некорректное значение или недоступный файл.
 */
void validateConfig(const TtsConfig &config);

} // namespace smart_home::tts
