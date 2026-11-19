#pragma once

#include "persistence/PersistenceTypes.h"

namespace smart_home::persistence {

class SaveWorker;

/**
 * @brief Подвижный токен одного слота очереди SaveWorker.
 * @details Токен используют последовательно. Воркер должен существовать
 * до освобождения всех выданных токенов.
 */
class Reservation {
  public:
    /** @brief Возвращает неиспользованный слот воркеру. */
    ~Reservation() noexcept;
    /** @brief Передаёт слот новому токену. */
    Reservation(Reservation &&other) noexcept;
    Reservation &
    operator=(Reservation &&) = delete; ///< Присваивание запрещено.

    /** @brief true, если токен владеет слотом. */
    explicit operator bool() const noexcept { return worker_ != nullptr; }

    /**
     * @brief Передаёт снимок в очередь и освобождает токен от владения слотом.
     * @throws std::logic_error Слот отсутствует, воркер остановлен или снимок
     * уже передан.
     */
    void submit(Snapshot &&snapshot);

  private:
    friend class SaveWorker;
    explicit Reservation(SaveWorker *worker) noexcept : worker_(worker) {}
    SaveWorker *worker_;
};

} // namespace smart_home::persistence
