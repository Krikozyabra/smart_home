#include "events/EventBus.h"
#include "EventBusState.h"

#include <exception>

namespace smart_home::events {

EventBus::EventBus() : state_(std::make_shared<detail::EventBusState>()) {}
EventBus::~EventBus() = default;

Subscription
EventBus::subscribeErased(std::type_index type,
                          std::function<void(const void *)> handler) {
    const auto entry = std::make_shared<detail::SubscriptionState>(
        type, std::move(handler), state_);
    const std::lock_guard<std::mutex> lock(state_->mutex);
    auto existing = state_->handlers.find(type);
    if (existing == state_->handlers.end())
        state_->handlers.emplace(type, std::vector{entry});
    else
        existing->second.push_back(entry);
    return Subscription(entry);
}

void EventBus::publishErased(detail::EventBusState &state, std::type_index type,
                             const void *event) {
    std::vector<std::shared_ptr<detail::SubscriptionState>> entries;
    {
        const std::lock_guard<std::mutex> lock(state.mutex);
        const auto found = state.handlers.find(type);
        if (found == state.handlers.end())
            return;
        entries = found->second;
    }
    std::exception_ptr error;
    for (const auto &entry : entries) {
        if (!entry->active.load())
            continue;
        try {
            entry->handler(event);
        } catch (...) {
            if (!error)
                error = std::current_exception();
        }
    }
    if (error)
        std::rethrow_exception(error);
}

std::size_t EventBus::subscriberCount() const noexcept {
    const std::lock_guard<std::mutex> lock(state_->mutex);
    std::size_t count = 0;
    for (const auto &entry : state_->handlers)
        count += entry.second.size();
    return count;
}

} // namespace smart_home::events
