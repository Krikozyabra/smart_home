/** @file AlsaAudioCapture.h
 * @brief Linux ALSA capture с RAII, ограниченным кольцом и PIMPL.
 */
#pragma once

#include "IAudioCapture.h"
#include <memory>

namespace smart_home::audio {
namespace detail {
struct TestAccess;
}

/** @brief Захват точного interleaved PCM16; собственный ресемплинг отсутствует.
 * @details Контракты потокобезопасности и ошибок заданы IAudioCapture.
 * После сбоя захвата для продолжения требуется новый экземпляр.
 */
class AlsaAudioCapture final : public IAudioCapture {
  public:
    /** @brief Проверяет настройки, открывает и настраивает PCM, резервирует
     * кольцо.
     * @throws std::invalid_argument Некорректная конфигурация.
     * @throws std::runtime_error Устройство недоступно или формат не
     * поддерживается.
     * @throws std::bad_alloc Недостаточно памяти.
     */
    explicit AlsaAudioCapture(AudioConfig config = {});
    ~AlsaAudioCapture() override; ///< Пробуждает и join worker без исключений.
    AlsaAudioCapture(const AlsaAudioCapture &) = delete; ///< Не копируется.
    AlsaAudioCapture &
    operator=(const AlsaAudioCapture &) = delete;   ///< Не копируется.
    AlsaAudioCapture(AlsaAudioCapture &&) = delete; ///< Не перемещается.
    AlsaAudioCapture &
    operator=(AlsaAudioCapture &&) = delete; ///< Не перемещается.

    AudioConfig getConfig() const override; ///< Копия настроек; см. интерфейс.
    void startContinuous() override;        ///< Новый сеанс; см. интерфейс.
    void
    stopContinuous() override; ///< Остановка и передача ошибки; см. интерфейс.
    PcmSamples
    recordFor(std::chrono::milliseconds duration) override; ///< См. интерфейс.
    AudioSnapshot
    snapshot() const override; ///< Независимый снимок; см. интерфейс.
    void rethrowIfFailed() const override; ///< Повтор ошибки; см. интерфейс.

  private:
    struct Impl;
    explicit AlsaAudioCapture(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend struct detail::TestAccess;
};

} // namespace smart_home::audio
