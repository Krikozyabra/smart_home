/**
 * @file Poller.h
 * @brief Ожидание готовности нескольких файловых дескрипторов Linux.
 */
#pragma once

#include <vector>

struct pollfd;

namespace smart_home::platform {

/** @brief Маска интересующих событий ввода-вывода. */
enum class PollEvent : unsigned {
    Read = 1,  ///< Готовность к чтению.
    Write = 2, ///< Готовность к записи.
};

/** @return Объединение двух масок интересующих событий. */
constexpr PollEvent operator|(PollEvent left, PollEvent right) noexcept {
    return static_cast<PollEvent>(static_cast<unsigned>(left) |
                                  static_cast<unsigned>(right));
}

/**
 * @brief Хранит список заимствованных дескрипторов для poll().
 * @details Ожидание не выделяет память и не удерживает блокировок. Один
 * экземпляр используют последовательно. Вызывающий код управляет временем жизни
 *          дескрипторов и сохраняет их номера на время опроса.
 */
class Poller {
  public:
    /** @brief Создаёт пустой список. */
    Poller() noexcept;
    /** @brief Освобождает список регистраций. */
    ~Poller();

    Poller(const Poller &) = delete;
    Poller &operator=(const Poller &) = delete;
    /** @brief Перемещает список регистраций и результаты последнего опроса. */
    Poller(Poller &&other) noexcept;
    /** @brief Заменяет список регистраций и результаты последнего опроса. */
    Poller &operator=(Poller &&other) noexcept;

    /**
     * @brief Регистрирует дескриптор для последующих опросов.
     * @param fd Дескриптор без передачи владения, уникальный в списке.
     * @param events Интерес к чтению, записи или обоим событиям.
     * @throws std::invalid_argument Отрицательный или повторный fd,
     * недопустимая маска.
     */
    void add(int fd, PollEvent events);

    /**
     * @brief Ожидает готовность и обновляет результаты. Повторяет вызов при
     * EINTR.
     * @param timeout_ms Общий таймаут в миллисекундах, 0 для немедленной
     * проверки, отрицательное значение для ожидания без таймаута.
     * @return Количество дескрипторов с событиями или 0 при истечении таймаута.
     * @throws std::system_error Ошибка poll().
     */
    int poll(int timeout_ms = -1);

    /** @return Признак POLLIN по последнему опросу. Для неизвестного fd
     * возвращает false. */
    bool isReadable(int fd) const noexcept;
    /** @return Признак POLLOUT по последнему опросу. Для неизвестного fd
     * возвращает false. */
    bool isWritable(int fd) const noexcept;
    /** @return Признак POLLHUP по последнему опросу. Для неизвестного fd
     * возвращает false. */
    bool isHangUp(int fd) const noexcept;
    /** @return Признак POLLERR по последнему опросу. Для неизвестного fd
     * возвращает false. */
    bool hasError(int fd) const noexcept;
    /** @return Признак POLLNVAL по последнему опросу. Для неизвестного fd
     * возвращает false. */
    bool isInvalid(int fd) const noexcept;

  private:
    const pollfd *find(int fd) const noexcept;
    short revents(int fd) const noexcept;

    std::vector<pollfd> descriptors_;
};

} // namespace smart_home::platform
