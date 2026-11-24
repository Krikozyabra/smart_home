/** @file ModelValidation.h
 * @brief Внутренние проверки модели и словаря перед вызовом sherpa-onnx.
 */
#pragma once

#include <cstddef>
#include <string_view>

namespace smart_home::stt {

struct SttConfig;

namespace detail {

inline constexpr std::size_t MinimumSamples = 1600; ///< 100 мс при 16000 Гц.

void validateModel(const SttConfig &config);
bool isValidUtf8(std::string_view text) noexcept;

} // namespace detail
} // namespace smart_home::stt
