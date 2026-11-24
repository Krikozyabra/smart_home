/**
 * @file SttResult.h
 * @brief Результат распознавания законченного аудиофрагмента.
 */
#pragma once

#include <string>
#include <vector>

namespace smart_home::stt {

/** @brief Собственные данные, действительные независимо от распознавателя. */
struct SttResult {
    std::string text;              ///< Распознанный текст в UTF-8.
    float duration_seconds = 0.0f; ///< Длительность входного аудио в секундах.
    std::vector<std::string> tokens; ///< Токены по порядку или пустой список.
    std::vector<float> timestamps; ///< Метки в секундах от начала, по одной на
                                   ///< токен при наличии.
};

} // namespace smart_home::stt
