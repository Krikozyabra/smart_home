#include "events/Reactor.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace smart_home::events {
namespace {

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

[[noreturn]] void systemError(const char *operation, int error = errno) {
    throw std::system_error(error, std::generic_category(), operation);
}

template <typename Operation>
auto retryInterrupted(Operation operation) noexcept {
    auto result = operation();
    while (result == -1 && errno == EINTR)
        result = operation();
    return result;
}

class Wakeup {
  public:
    Wakeup() : fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
        if (fd_ < 0)
            systemError("eventfd");
    }
    ~Wakeup() noexcept { ::close(fd_); }
    Wakeup(const Wakeup &) = delete;
    Wakeup &operator=(const Wakeup &) = delete;

    int fd() const noexcept { return fd_; }

    int notify() const noexcept {
        const std::uint64_t increment = 1;
        const auto result = retryInterrupted(
            [&] { return ::write(fd_, &increment, sizeof(increment)); });
        return result < 0 && errno != EAGAIN ? errno : 0;
    }

    void drain() const {
        std::uint64_t pending;
        const auto result = retryInterrupted(
            [&] { return ::read(fd_, &pending, sizeof(pending)); });
        if (result < 0 && errno != EAGAIN)
            systemError("read eventfd");
    }

  private:
    int fd_;
};

class LoopScope {
  public:
    LoopScope(std::atomic<bool> &running, std::atomic<bool> &stopping)
        : running_(running), stopping_(stopping) {
        if (running_.exchange(true))
            throw std::logic_error("Reactor loop is already active.");
    }
    ~LoopScope() noexcept {
        stopping_.store(false);
        running_.store(false);
    }
    LoopScope(const LoopScope &) = delete;
    LoopScope &operator=(const LoopScope &) = delete;

  private:
    std::atomic<bool> &running_;
    std::atomic<bool> &stopping_;
};

constexpr auto exceptional_events = IoEvent::HangUp | IoEvent::Error;
constexpr auto valid_events =
    IoEvent::Read | IoEvent::Write | exceptional_events;

bool valid(IoEvent events) noexcept {
    return (events & valid_events) == events;
}

short pollMask(IoEvent events) noexcept {
    short mask = 0;
    if ((events & IoEvent::Read) != IoEvent::None)
        mask |= POLLIN;
    if ((events & IoEvent::Write) != IoEvent::None)
        mask |= POLLOUT;
    return mask;
}

IoEvent readyEvents(short mask) noexcept {
    IoEvent events = IoEvent::None;
    if (mask & POLLIN)
        events = events | IoEvent::Read;
    if (mask & POLLOUT)
        events = events | IoEvent::Write;
    if (mask & POLLHUP)
        events = events | IoEvent::HangUp;
    if (mask & (POLLERR | POLLNVAL))
        events = events | IoEvent::Error;
    return events;
}

Clock::time_point after(Clock::time_point now, Clock::duration delay) noexcept {
    if (delay > Clock::time_point::max() - now)
        return Clock::time_point::max();
    return now + delay;
}

int pollTimeout(const std::optional<Clock::time_point> &deadline) noexcept {
    if (!deadline)
        return -1;
    const auto remaining =
        std::chrono::ceil<Milliseconds>(*deadline - Clock::now());
    return static_cast<int>(
        std::clamp(remaining, Milliseconds::zero(),
                   Milliseconds(std::numeric_limits<int>::max()))
            .count());
}

} // namespace

struct detail::TimerState {
    TimerState(Clock::time_point due, Clock::duration interval, bool periodic,
               Reactor::Task callback)
        : due(due), interval(interval), periodic(periodic),
          callback(std::move(callback)) {}

    Clock::time_point due;
    const Clock::duration interval;
    const bool periodic;
    const Reactor::Task callback;
    std::atomic<bool> active{true};
};

TimerHandle::TimerHandle(std::weak_ptr<detail::TimerState> state) noexcept
    : state_(std::move(state)) {}
TimerHandle::~TimerHandle() noexcept { cancel(); }
TimerHandle::TimerHandle(TimerHandle &&other) noexcept = default;

TimerHandle &TimerHandle::operator=(TimerHandle &&other) noexcept {
    if (this != &other) {
        cancel();
        state_ = std::move(other.state_);
    }
    return *this;
}

void TimerHandle::cancel() noexcept {
    if (const auto timer = state_.lock())
        timer->active.store(false);
}

bool TimerHandle::isActive() const noexcept {
    const auto timer = state_.lock();
    return timer && timer->active.load();
}

class Reactor::Impl {
  public:
    explicit Impl(ErrorHandler error_handler)
        : on_error_(std::move(error_handler)) {}

    void addHandler(int fd, IoEvent events, IoHandler handler) {
        if (fd < 0 || !valid(events) || !handler ||
            findHandler(fd) != handlers_.end())
            throw std::invalid_argument(
                "Invalid or duplicate reactor I/O registration.");
        handlers_.push_back(
            std::make_shared<Handler>(fd, events, std::move(handler)));
    }

    void modifyHandler(int fd, IoEvent events) {
        const auto found = findHandler(fd);
        if (found == handlers_.end() || !valid(events))
            throw std::invalid_argument(
                "Unknown reactor descriptor or invalid event mask.");
        (*found)->events = events;
    }

    void removeHandler(int fd) noexcept {
        const auto found = findHandler(fd);
        if (found != handlers_.end()) {
            const auto removed = std::move(*found);
            removed->active = false;
            handlers_.erase(found);
        }
    }

    std::shared_ptr<detail::TimerState> addTimer(Milliseconds interval,
                                                 bool periodic, Task callback) {
        if (interval.count() < 0 || (periodic && interval.count() == 0) ||
            !callback ||
            interval > std::chrono::duration_cast<Milliseconds>(
                           Clock::duration::max()))
            throw std::invalid_argument(
                "Invalid reactor timer interval or callback.");
        const auto timer = std::make_shared<detail::TimerState>(
            after(Clock::now(), interval), interval, periodic,
            std::move(callback));
        timers_.push_back(timer);
        return timer;
    }

    void post(Task task) {
        if (!task)
            throw std::invalid_argument("Reactor requires a task.");
        int error;
        {
            const std::lock_guard<std::mutex> lock(task_mutex_);
            tasks_.emplace_back();
            tasks_.back().swap(task);
            error = wakeup_.notify();
            if (error != 0) {
                tasks_.back().swap(task);
                tasks_.pop_back();
            }
        }
        if (error != 0)
            systemError("write eventfd", error);
    }

    void stop() noexcept {
        stopping_.store(true);
        wakeup_.notify();
    }

    void run() {
        const LoopScope scope(running_, stopping_);
        while (!stopping_.load())
            iterate(-1);
    }

    int step(int timeout_ms) {
        const LoopScope scope(running_, stopping_);
        return iterate(timeout_ms);
    }

    bool isRunning() const noexcept { return running_.load(); }

  private:
    struct Handler {
        Handler(int fd, IoEvent events, IoHandler callback)
            : fd(fd), events(events), callback(std::move(callback)) {}

        int fd;
        IoEvent events;
        IoHandler callback;
        bool active = true;
    };
    using Handlers = std::vector<std::shared_ptr<Handler>>;

    Handlers::iterator findHandler(int fd) noexcept {
        return std::find_if(
            handlers_.begin(), handlers_.end(),
            [fd](const auto &handler) { return handler->fd == fd; });
    }

    void removeInactiveTimers() {
        // Сначала обновляем реестр, затем освобождаем пользовательские
        // коллбэки.
        decltype(timers_) removed;
        for (auto timer = timers_.begin(); timer != timers_.end();) {
            const auto current = timer++;
            if (!(*current)->active.load())
                removed.splice(removed.end(), timers_, current);
        }
    }

    std::size_t taskCount() {
        const std::lock_guard<std::mutex> lock(task_mutex_);
        return tasks_.size();
    }

    Task takeTask() {
        Task task;
        const std::lock_guard<std::mutex> lock(task_mutex_);
        task.swap(tasks_.front());
        tasks_.pop_front();
        return task;
    }

    void waitForEvents(const std::optional<Clock::time_point> &deadline) {
        for (;;) {
            if (::poll(poll_fds_.data(), poll_fds_.size(),
                       pollTimeout(deadline)) >= 0)
                return;
            if (errno != EINTR)
                systemError("poll reactor");
            if (stopping_.load() || (deadline && *deadline <= Clock::now())) {
                for (auto &fd : poll_fds_)
                    fd.revents = 0;
                return;
            }
        }
    }

    template <typename Callback, typename... Args>
    void invoke(const Callback &callback, Args... args) noexcept {
        try {
            callback(args...);
        } catch (...) {
            if (on_error_) {
                try {
                    on_error_(std::current_exception());
                } catch (...) {
                    // Сбой получателя ошибок не должен остановить реактор.
                }
            }
        }
    }

    int iterate(int timeout_ms) {
        if (stopping_.load())
            return 0;
        removeInactiveTimers();
        const auto now = Clock::now();
        std::optional<Clock::time_point> deadline;
        if (timeout_ms >= 0)
            deadline = after(now, Milliseconds(timeout_ms));
        if (taskCount() != 0)
            deadline = now;
        for (const auto &timer : timers_) {
            if (!deadline || timer->due < *deadline)
                deadline = timer->due;
        }

        const auto handlers = handlers_;
        poll_fds_.clear();
        poll_fds_.reserve(handlers.size() + 1);
        poll_fds_.push_back({wakeup_.fd(), POLLIN, 0});
        for (const auto &handler : handlers)
            poll_fds_.push_back({handler->fd, pollMask(handler->events), 0});
        waitForEvents(deadline);
        const auto wake_events = poll_fds_.front().revents;
        if (wake_events & (POLLERR | POLLHUP | POLLNVAL))
            throw std::runtime_error("Reactor eventfd is unavailable.");
        if (wake_events & POLLIN)
            wakeup_.drain();

        const auto tasks_ready = taskCount();
        const auto ready_at = Clock::now();
        std::vector<std::shared_ptr<detail::TimerState>> due_timers;
        for (const auto &timer : timers_) {
            if (timer->due <= ready_at)
                due_timers.push_back(timer);
        }

        std::size_t dispatched = 0;
        for (std::size_t i = 0; i < handlers.size() && !stopping_.load(); ++i) {
            const auto &handler = handlers[i];
            const auto events = readyEvents(poll_fds_[i + 1].revents) &
                                (handler->events | exceptional_events);
            if (handler->active && events != IoEvent::None) {
                ++dispatched;
                invoke(handler->callback, handler->fd, events);
            }
        }
        for (std::size_t i = 0; i < tasks_ready && !stopping_.load(); ++i) {
            ++dispatched;
            invoke(takeTask());
        }
        for (const auto &timer : due_timers) {
            if (stopping_.load())
                break;
            const bool active = timer->periodic ? timer->active.load()
                                                : timer->active.exchange(false);
            if (active) {
                ++dispatched;
                invoke(timer->callback);
                if (timer->periodic)
                    timer->due = after(Clock::now(), timer->interval);
            }
        }
        removeInactiveTimers();
        return static_cast<int>(std::min(
            dispatched,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));
    }

    Wakeup wakeup_;
    ErrorHandler on_error_;
    Handlers handlers_;
    std::list<std::shared_ptr<detail::TimerState>> timers_;
    std::vector<pollfd> poll_fds_;
    std::mutex task_mutex_;
    std::deque<Task> tasks_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
};

Reactor::Reactor(ErrorHandler on_error)
    : impl_(std::make_unique<Impl>(std::move(on_error))) {}
Reactor::~Reactor() noexcept = default;

void Reactor::addHandler(int fd, IoEvent events, IoHandler handler) {
    impl_->addHandler(fd, events, std::move(handler));
}

void Reactor::modifyHandler(int fd, IoEvent events) {
    impl_->modifyHandler(fd, events);
}
void Reactor::removeHandler(int fd) noexcept { impl_->removeHandler(fd); }

TimerHandle Reactor::addTimer(std::chrono::milliseconds interval, bool periodic,
                              Task callback) {
    return TimerHandle(
        impl_->addTimer(interval, periodic, std::move(callback)));
}

void Reactor::post(Task task) { impl_->post(std::move(task)); }
void Reactor::run() { impl_->run(); }
int Reactor::step(int timeout_ms) { return impl_->step(timeout_ms); }
void Reactor::stop() noexcept { impl_->stop(); }
bool Reactor::isRunning() const noexcept { return impl_->isRunning(); }

} // namespace smart_home::events
