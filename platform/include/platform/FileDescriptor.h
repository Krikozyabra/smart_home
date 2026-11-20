/**
 * @file FileDescriptor.h
 * @brief Эксклюзивное владение файловым дескриптором Linux.
 */
#pragma once

namespace smart_home::platform {

/**
 * @brief Закрывает принадлежащий объекту дескриптор при разрушении.
 * @details Перемещённый объект становится пустым. Доступ к одному экземпляру
 *          при смене владельца и закрытии синхронизирует вызывающий код.
 */
class FileDescriptor {
  public:
    /** @param fd Передаваемый во владение дескриптор. Отрицательное значение
     * задаёт пустой объект. */
    explicit FileDescriptor(int fd = -1) noexcept;
    /** @brief Закрывает дескриптор без исключений. */
    ~FileDescriptor() noexcept;

    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    /** @brief Забирает дескриптор у другого объекта. */
    FileDescriptor(FileDescriptor &&other) noexcept;
    /** @brief Закрывает прежний дескриптор без исключений и забирает новый. */
    FileDescriptor &operator=(FileDescriptor &&other) noexcept;

    /** @return Заимствованный дескриптор или -1. Действует до смены владельца
     * или закрытия. */
    int get() const noexcept { return fd_; }
    /** @return Признак того, что объект владеет дескриптором. */
    bool valid() const noexcept { return fd_ >= 0; }
    /** @return Признак того, что объект владеет дескриптором. */
    explicit operator bool() const noexcept { return valid(); }

    /** @return Дескриптор, переданный вызывающему коду, или -1 для пустого
     * объекта. */
    int release() noexcept;

    /**
     * @brief Заменяет дескриптор и закрывает прежний при несовпадении номеров.
     * @param fd Новый дескриптор во владение или отрицательное значение для
     * сброса.
     * @throws std::system_error Ошибка закрытия прежнего дескриптора. Объект
     * уже владеет новым.
     */
    void reset(int fd = -1);

    /**
     * @brief Закрывает дескриптор. Пустой объект остаётся пустым.
     * @throws std::system_error Ошибка close(). Объект остаётся пустым и при
     * ошибке.
     */
    void close();

  private:
    int fd_;
};

} // namespace smart_home::platform
