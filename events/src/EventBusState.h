/**
 * @file EventBusState.h
 * @brief Внутренний реестр подписок и записи обработчиков EventBus.
 */
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace smart_home::events::detail {

struct EventBusState;

struct SubscriptionState {
    SubscriptionState(std::type_index type,
                      std::function<void(const void *)> handler,
                      const std::shared_ptr<EventBusState> &owner)
        : type(type), handler(std::move(handler)), owner(owner) {}

    const std::type_index type;
    const std::function<void(const void *)> handler;
    const std::weak_ptr<EventBusState> owner;
    std::atomic<bool> active{true};
};

struct EventBusState {
    std::mutex mutex;
    std::unordered_map<std::type_index,
                       std::vector<std::shared_ptr<SubscriptionState>>>
        handlers;
};

} // namespace smart_home::events::detail
