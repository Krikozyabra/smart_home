/**
 * @file TimerHandle.h
 * @brief Токен регистрации монотонного таймера реактора.
 */
#pragma once

#include <memory>

namespace smart_home::events {
namespace detail {
struct TimerState;
}

/**
 * @brief Отменяет таймер при уничтожении. Может пережить реактор.
 * @details cancel() и isActive() потокобезопасны. Перемещение и уничтожение
 *          токена синхронизирует вызывающий код.
 */
class TimerHandle {
  public:
    /** @brief Создаёт пустой токен. */
    TimerHandle() noexcept = default;
    /** @brief Отменяет зарегистрированный таймер. */
    ~TimerHandle() noexcept;

    TimerHandle(const TimerHandle &) = delete;
    TimerHandle &operator=(const TimerHandle &) = delete;
    /** @brief Передаёт таймер, оставляя исходный токен пустым. */
    TimerHandle(TimerHandle &&other) noexcept;
    /** @brief Отменяет прежний таймер и принимает таймер другого токена. */
    TimerHandle &operator=(TimerHandle &&other) noexcept;

    /**
     * @brief Отменяет последующие срабатывания. Повторный вызов безопасен.
     * @details Уже допущенный вызов может завершиться после отмены. Реактор
     *          освобождает отменённые таймеры в ходе диспетчеризации.
     */
    void cancel() noexcept;

    /** @return Признак активного таймера. Разовый таймер отключается перед
     * вызовом. */
    bool isActive() const noexcept;

  private:
    friend class Reactor;
    explicit TimerHandle(std::weak_ptr<detail::TimerState> state) noexcept;

    std::weak_ptr<detail::TimerState> state_;
};

} // namespace smart_home::events
