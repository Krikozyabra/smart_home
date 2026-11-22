/**
 * @file EventBus.h
 * @brief Синхронная внутрипроцессная шина типизированных событий.
 */
#pragma once

#include "events/Subscription.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace smart_home::events {
namespace detail {
struct EventBusState;
}

/**
 * @brief Доставляет событие подписчикам его статического типа в порядке
 * регистрации.
 * @details Реестр потокобезопасен. Вложенные операции выполняются без
 * блокировки реестра на время пользовательского кода. Одновременные публикации
 * могут вызвать обработчик параллельно, поэтому он сам синхронизирует своё
 * состояние. Вызывающий код синхронизирует уничтожение шины с прямыми вызовами
 * её методов.
 */
class EventBus {
  public:
    /** @brief Создаёт пустой реестр подписок. */
    EventBus();
    /** @brief Освобождает реестр подписок. */
    ~EventBus();

    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;

    /**
     * @brief Добавляет обработчик в конец списка подписчиков типа Event.
     * @tparam Event Тип данных события без обязательного базового класса.
     * @param handler Обработчик, получающий заимствованную ссылку на время
     * вызова.
     * @return Токен, управляющий временем жизни подписки.
     * @throws std::invalid_argument Передан пустой обработчик.
     */
    template <typename Event>
    Subscription subscribe(std::function<void(const Event &)> handler) {
        if (!handler)
            throw std::invalid_argument(
                "Event subscription requires a handler.");
        return subscribeErased(
            typeid(Event), [handler = std::move(handler)](const void *value) {
                handler(*static_cast<const Event *>(value));
            });
    }

    /**
     * @brief Вызывает подписчиков данного типа на потоке вызывающего кода.
     * @param event Заимствованное событие, действительное до возврата из
     * метода.
     * @details Новые подписки видны в следующих публикациях, включая вложенные.
     *          Отменённые подписки пропускаются, если доставка ещё не началась.
     *          После обхода списка пробрасывается первое исключение
     * обработчика.
     */
    template <typename Event> void publish(const Event &event) {
        publishErased(*state_, typeid(Event), std::addressof(event));
    }

    /** @return Текущее число подписок по всем типам. */
    std::size_t subscriberCount() const noexcept;

  private:
    friend class Reactor;
    Subscription subscribeErased(std::type_index type,
                                 std::function<void(const void *)> handler);
    static void publishErased(detail::EventBusState &state,
                              std::type_index type, const void *event);

    std::shared_ptr<detail::EventBusState> state_;
};

} // namespace smart_home::events
