/**
 * @file SttConfig.h
 * @brief Конфигурация локального распознавания речи GigaAM-v3 CTC.
 */
#pragma once

#include <string>

namespace smart_home::stt {

/** @brief Настройки модели, формата аудио и выполнения на CPU. */
struct SttConfig {
    std::string
        model_path; ///< Путь к модели GigaAM-v3 CTC ONNX, FP32 или INT8.
    std::string tokens_path; ///< Путь к словарю, выпущенному для этой модели.
    int sample_rate_hz = 16000; ///< Частота входных отсчётов, 16000 Гц.
    int feature_dim = 64;       ///< Число полос Fbank, 64.
    int num_threads = 2;        ///< Число потоков CPU, от 1 до 4 включительно.
    std::string decoding_method = "greedy_search"; ///< Метод декодирования CTC.
};

/**
 * @brief Проверяет параметры и доступность файлов до создания сессий SDK.
 * @param config Проверяемая конфигурация. Оба пути должны указывать на
 *        читаемые непустые обычные файлы.
 * @throws std::invalid_argument Некорректный параметр или недоступный файл.
 * @details Совместимость формата модели и словаря проверяет распознаватель
 *          при создании.
 */
void validateConfig(const SttConfig &config);

} // namespace smart_home::stt
