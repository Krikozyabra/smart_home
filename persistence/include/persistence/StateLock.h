#pragma once

#include <filesystem>

namespace smart_home::persistence {

/** @brief Владеет эксклюзивной блокировкой каталога состояния процесса. */
class StateLock {
  public:
    /**
     * @brief Готовит каталог состояния и захватывает runtime.lock через flock.
     * @param state_dir Каталог на локальной файловой системе с rename и fsync.
     * @throws std::runtime_error Каталог недоступен или уже удерживается
     * писателем.
     */
    explicit StateLock(const std::filesystem::path &state_dir);
    /** @brief Освобождает блокировку закрытием дескриптора. */
    ~StateLock() noexcept;
    StateLock(const StateLock &) = delete; ///< Копирование запрещено.
    StateLock &
    operator=(const StateLock &) = delete; ///< Присваивание запрещено.

    /**
     * @brief Возвращает абсолютный путь к заблокированному каталогу.
     * @return Ссылка действительна до уничтожения блокировки.
     */
    const std::filesystem::path &directory() const noexcept {
        return directory_;
    }

  private:
    std::filesystem::path directory_;
    int fd_;
};

} // namespace smart_home::persistence
