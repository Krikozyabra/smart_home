/**
 * @file Subscription.h
 * @brief Токен регистрации типизированного обработчика.
 */
#pragma once

#include <memory>

namespace smart_home::events {
namespace detail {
struct SubscriptionState;
}

/**
 * @brief Отзывает подписку при уничтожении. Может пережить шину событий.
 * @details Перемещение передаёт подписку. cancel() и isSubscribed() допускают
 *          одновременные вызовы. Перемещение и уничтожение токена
 * синхронизирует вызывающий код.
 */
class Subscription {
  public:
    /** @brief Создаёт пустой токен. */
    Subscription() noexcept = default;
    /** @brief Отзывает принадлежащую токену подписку. */
    ~Subscription() noexcept;

    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;
    /** @brief Передаёт подписку, оставляя исходный токен пустым. */
    Subscription(Subscription &&other) noexcept;
    /** @brief Отзывает прежнюю подписку и принимает подписку другого токена. */
    Subscription &operator=(Subscription &&other) noexcept;

    /**
     * @brief Удаляет подписку из реестра. Повторный вызов безопасен.
     * @details Доставка, уже допущенная до отмены, может завершиться после неё.
     */
    void cancel() noexcept;

    /** @return Признак активной подписки на момент проверки. */
    bool isSubscribed() const noexcept;

  private:
    friend class EventBus;
    explicit Subscription(
        std::weak_ptr<detail::SubscriptionState> state) noexcept;

    std::weak_ptr<detail::SubscriptionState> state_;
};

} // namespace smart_home::events
