/** @file OwwConfig.h
 * @brief Настройки потокового детектора OpenWakeWord.
 */
#pragma once

#include <string>

namespace smart_home::oww {

/** @brief Файлы ONNX и ограничения CPU для одного ключевого слова. */
struct OwwConfig {
    std::string model_path; ///< ONNX-классификатор слова, например «Нафаня».
    int sample_rate_hz =
        16000; ///< Частота PCM; поддерживается только 16000 Гц.
    int inference_threads =
        1; ///< Число потоков внутри операции инференса; минимум 1.
    std::string
        melspec_model_path; ///< Пустой: melspectrogram.onnx рядом с model_path.
    std::string embedding_model_path; ///< Пустой: embedding_model.onnx рядом с
                                      ///< model_path.
};

/** @brief Проверяет настройки и наличие всех трёх обычных файлов моделей.
 * @param config Настройки; файлы не изменяются и модели не загружаются.
 * @throws std::invalid_argument Пустой/недоступный путь, отсутствующий файл,
 * неподдерживаемая частота или неположительное число потоков.
 */
void validateConfig(const OwwConfig &config);

} // namespace smart_home::oww
