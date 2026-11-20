#include "platform/Poller.h"
#include "platform/SystemError.h"

#include <poll.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace smart_home::platform {

Poller::Poller() noexcept = default;
Poller::~Poller() = default;
Poller::Poller(Poller &&other) noexcept = default;
Poller &Poller::operator=(Poller &&other) noexcept = default;

void Poller::add(int fd, PollEvent events) {
    const auto mask = static_cast<unsigned>(events);
    constexpr auto read = static_cast<unsigned>(PollEvent::Read);
    constexpr auto write = static_cast<unsigned>(PollEvent::Write);
    if (fd < 0 || mask == 0 || (mask & ~(read | write)) != 0)
        throw std::invalid_argument("Invalid poll descriptor or event mask.");
    if (find(fd))
        throw std::invalid_argument("Poll descriptor is already registered.");
    short flags = 0;
    if (mask & read)
        flags |= POLLIN;
    if (mask & write)
        flags |= POLLOUT;
    descriptors_.push_back({fd, flags, 0});
}

int Poller::poll(int timeout_ms) {
    using Clock = std::chrono::steady_clock;
    using std::chrono::milliseconds;
    const bool timed = timeout_ms >= 0;
    const auto deadline =
        timed ? Clock::now() + milliseconds(timeout_ms) : Clock::time_point{};
    for (;;) {
        const int result =
            ::poll(descriptors_.data(), descriptors_.size(), timeout_ms);
        if (result >= 0)
            return result;
        if (errno != EINTR)
            throwSystemError("poll");
        if (timed) {
            const auto remaining = deadline - Clock::now();
            if (remaining <= Clock::duration::zero()) {
                for (auto &descriptor : descriptors_)
                    descriptor.revents = 0;
                return 0;
            }
            timeout_ms = static_cast<int>(
                std::chrono::ceil<milliseconds>(remaining).count());
        }
    }
}

const pollfd *Poller::find(int fd) const noexcept {
    const auto found =
        std::find_if(descriptors_.begin(), descriptors_.end(),
                     [fd](const pollfd &entry) { return entry.fd == fd; });
    return found == descriptors_.end() ? nullptr : &*found;
}

short Poller::revents(int fd) const noexcept {
    const auto *entry = find(fd);
    return entry ? entry->revents : 0;
}

bool Poller::isReadable(int fd) const noexcept {
    return (revents(fd) & POLLIN) != 0;
}
bool Poller::isWritable(int fd) const noexcept {
    return (revents(fd) & POLLOUT) != 0;
}
bool Poller::isHangUp(int fd) const noexcept {
    return (revents(fd) & POLLHUP) != 0;
}
bool Poller::hasError(int fd) const noexcept {
    return (revents(fd) & POLLERR) != 0;
}
bool Poller::isInvalid(int fd) const noexcept {
    return (revents(fd) & POLLNVAL) != 0;
}

} // namespace smart_home::platform
