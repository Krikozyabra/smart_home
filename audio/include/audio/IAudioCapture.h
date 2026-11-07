/** @file IAudioCapture.h
 * @brief Контракт разового и непрерывного захвата одного микрофона.
 */
#pragma once

#include "AudioConfig.h"
#include "AudioTypes.h"

namespace smart_home::audio {

/** @brief Потокобезопасный захват с одним владельцем PCM текущего режима.
 * @details Читателей может быть несколько. Управляющие операции сериализованы;
 * конфликт с разовой записью отклоняется. Перед уничтожением объекта завершите
 * все внешние обращения. Возвращаемые данные не ссылаются на память захвата.
 */
class IAudioCapture {
  public:
    virtual ~IAudioCapture() = default; ///< Освобождает ресурсы без исключений.
    /** @brief Возвращает собственную копию неизменяемых настроек. */
    virtual AudioConfig getConfig() const = 0;
    /** @brief Запускает worker и новый пустой сеанс; повтор во время записи —
     * no-op.
     * @throws std::logic_error Активна разовая запись.
     * @throws std::runtime_error Сохранённая ошибка захвата или предел
     * session_id.
     * @throws std::system_error Не удалось создать поток; запуск откатывается.
     */
    virtual void startContinuous() = 0;
    /** @brief Пробуждает и присоединяет worker; повтор после успеха — no-op.
     * @throws std::logic_error Активна разовая запись.
     * @throws std::runtime_error Сохранённая ошибка (после join).
     */
    virtual void stopContinuous() = 0;
    /** @brief Синхронно записывает duration, не меняя кольцо, возвращает PCM
     * целиком.
     * @param duration Положительное число мс, дающее целое число отсчётов.
     * @throws std::invalid_argument Некорректная длительность или размер.
     * @throws std::logic_error Любой режим записи уже занят.
     * @throws std::runtime_error Сбой захвата; объект остаётся в Failed.
     * @details Короткий последний фрагмент включён. Время вызова может
     * превышать duration. Ошибка аргументов не переводит объект в Failed.
     */
    virtual PcmSamples recordFor(std::chrono::milliseconds duration) = 0;
    /** @brief Копирует полные кадры, в том числе после остановки или сбоя.
     * @details Блокирует публикацию только на время копирования; до запуска
     * пуст.
     */
    virtual AudioSnapshot snapshot() const = 0;
    /** @brief Повторяет сохранённое исключение worker/разовой записи без
     * очистки.
     * @details Не ждёт worker; при отсутствии ошибки ничего не делает.
     */
    virtual void rethrowIfFailed() const = 0;
};

} // namespace smart_home::audio
