/** @file IOpenWakeWord.h
 * @brief Интерфейс обработки последовательных кадров PCM без копирования на
 * входе.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smart_home::oww {

inline constexpr std::size_t FrameSamples =
    1280; ///< 80 мс моно PCM при 16000 Гц.
using PcmFrame =
    std::array<std::int16_t, FrameSamples>; ///< Один кадр signed PCM16.

/** @brief Потоковый детектор одного ключевого слова; экземпляр хранит
 * аудиоконтекст.
 * @details Вызовы одного экземпляра выполняются последовательно для одного
 * потока аудио. Для независимых потоков нужны отдельные экземпляры.
 */
class IOpenWakeWord {
  public:
    virtual ~IOpenWakeWord() = default; ///< Освобождает ресурсы детектора.

    /** @brief Возвращает исходный путь к классификатору ключевого слова.
     * @return Ссылка действительна до уничтожения детектора.
     */
    virtual const std::string &getModelPath() const noexcept = 0;

    /** @brief Обрабатывает очередные 80 мс моно PCM16 без проверки длины.
     * @param pcm_frame Указатель на 1280 доступных отсчётов с частотой 16000
     * Гц.
     * @pre pcm_frame не равен nullptr и указывает на полный кадр. Проверку
     * границ обеспечивает вызывающая сторона; неполный буфер нарушает контракт.
     * @return Конечная вероятность в диапазоне [0, 1].
     * @throws std::runtime_error Ошибка инференса или некорректный результат
     * модели.
     * @details Контекст до первого кадра заполняется тишиной при создании
     * детектора.
     */
    virtual float processFrame(const std::int16_t *pcm_frame) = 0;

    /** @brief Обрабатывает кадр фиксированного размера.
     * @param pcm_frame Кадр из 1280 отсчётов.
     * @return Вероятность ключевого слова в диапазоне [0, 1].
     */
    float processFrame(const PcmFrame &pcm_frame) {
        return processFrame(pcm_frame.data());
    }

    /** @brief Передаёт данные вектора без копирования и проверки его размера.
     * @param pcm_frame Вектор с 1280 доступными отсчётами; это предусловие
     * вызова.
     * @return Вероятность ключевого слова в диапазоне [0, 1].
     */
    float processFrame(const std::vector<std::int16_t> &pcm_frame) {
        return processFrame(pcm_frame.data());
    }
};

} // namespace smart_home::oww
