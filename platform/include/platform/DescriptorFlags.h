/**
 * @file DescriptorFlags.h
 * @brief Временная установка неблокирующего режима дескриптора.
 */
#pragma once

namespace smart_home::platform {

/**
 * @brief Устанавливает O_NONBLOCK и восстанавливает исходные флаги.
 * @details Объект заимствует дескриптор на всё время действия режима. Флаги
 *          относятся к открытому файлу и действуют для dup() этого дескриптора.
 *          Вызывающий код сохраняет дескриптор открытым и синхронизирует смену
 * флагов.
 */
class NonBlockingScope {
  public:
    /**
     * @param fd Открытый дескриптор без передачи владения.
     * @throws std::invalid_argument Отрицательный дескриптор.
     * @throws std::system_error Ошибка чтения или установки флагов fcntl().
     */
    explicit NonBlockingScope(int fd);
    /** @brief Восстанавливает исходные флаги без исключений. */
    ~NonBlockingScope() noexcept;

    NonBlockingScope(const NonBlockingScope &) = delete;
    NonBlockingScope &operator=(const NonBlockingScope &) = delete;
    /** @brief Передаёт обязанность восстановления флагов. */
    NonBlockingScope(NonBlockingScope &&other) noexcept;
    /** @brief Восстанавливает прежние флаги без исключений и принимает новый
     * дескриптор. */
    NonBlockingScope &operator=(NonBlockingScope &&other) noexcept;

    /**
     * @brief Восстанавливает флаги досрочно. Повторный вызов после успеха
     * безопасен.
     * @throws std::system_error Ошибка fcntl(). Восстановление можно повторить.
     */
    void restore();

  private:
    int restoreFlags() noexcept;

    int fd_;
    int saved_flags_;
};

} // namespace smart_home::platform
