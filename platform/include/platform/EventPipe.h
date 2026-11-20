/**
 * @file EventPipe.h
 * @brief Неблокирующий канал уведомлений для опроса через poll().
 */
#pragma once

#include "platform/FileDescriptor.h"

namespace smart_home::platform {

/**
 * @brief Владеет обоими концами канала pipe2 с O_NONBLOCK и O_CLOEXEC.
 * @details Несколько потоков могут отправлять уведомления одному читателю.
 *          Перемещение и разрушение требуют завершения всех обращений к
 * объекту.
 */
class EventPipe {
  public:
    /** @throws std::system_error Ошибка создания pipe2(). */
    EventPipe();

    EventPipe(const EventPipe &) = delete;
    EventPipe &operator=(const EventPipe &) = delete;
    /** @brief Передаёт владение обоими концами канала. */
    EventPipe(EventPipe &&) noexcept = default;
    /** @brief Закрывает прежний канал и принимает оба конца нового. */
    EventPipe &operator=(EventPipe &&) noexcept = default;

    /** @return Заимствованный дескриптор чтения до перемещения или разрушения
     * объекта. */
    int readerFd() const noexcept { return reader_.get(); }

    /**
     * @brief Записывает байт пробуждения. Потокобезопасно и async-signal-safe.
     * @return 0 после записи или если канал заполнен, иначе код errno.
     * @note Сохраняет errno вызывающего. Несколько уведомлений могут
     * объединиться.
     */
    int notify() const noexcept;

    /**
     * @brief Считывает все доступные байты до EAGAIN. Вызывается единственным
     * читателем.
     * @throws std::system_error Ошибка read().
     * @throws std::runtime_error Конец записи неожиданно закрыт.
     */
    void drain() const;

  private:
    friend class SignalHandler;

    FileDescriptor reader_;
    FileDescriptor writer_;
};

} // namespace smart_home::platform
