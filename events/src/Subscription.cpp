#include "events/Subscription.h"
#include "EventBusState.h"

#include <algorithm>

namespace smart_home::events {

Subscription::Subscription(
    std::weak_ptr<detail::SubscriptionState> state) noexcept
    : state_(std::move(state)) {}

Subscription::~Subscription() noexcept { cancel(); }

Subscription::Subscription(Subscription &&other) noexcept = default;

Subscription &Subscription::operator=(Subscription &&other) noexcept {
    if (this != &other) {
        cancel();
        state_ = std::move(other.state_);
    }
    return *this;
}

void Subscription::cancel() noexcept {
    // Держим объект обработчика до выхода из секции с мьютексом.
    const auto entry = state_.lock();
    if (!entry)
        return;
    const auto owner = entry->owner.lock();
    if (!owner)
        return;
    const std::lock_guard<std::mutex> lock(owner->mutex);
    if (!entry->active.exchange(false))
        return;
    const auto found = owner->handlers.find(entry->type);
    auto &handlers = found->second;
    handlers.erase(std::find(handlers.begin(), handlers.end(), entry));
    if (handlers.empty())
        owner->handlers.erase(found);
}

bool Subscription::isSubscribed() const noexcept {
    const auto entry = state_.lock();
    return entry && !entry->owner.expired() && entry->active.load();
}

} // namespace smart_home::events
