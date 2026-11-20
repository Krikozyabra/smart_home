#include "platform/SignalHandler.h"
#include "EventPipeInternal.h"
#include "platform/EventPipe.h"
#include "platform/SystemError.h"

#include <signal.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <utility>

namespace smart_home::platform {
namespace {

static_assert(std::atomic<bool>::is_always_lock_free &&
                  std::atomic<int>::is_always_lock_free,
              "Signal notification requires lock-free atomics.");
std::atomic<bool> stop_requested{false};
std::atomic<int> signal_writer{-1};

int requestStopAndNotify() noexcept {
    stop_requested.store(true, std::memory_order_relaxed);
    return detail::notifyEvent(signal_writer.load(std::memory_order_relaxed));
}

extern "C" {
static void signalDispatcher(int) noexcept { (void)requestStopAndNotify(); }
}

} // namespace

struct SignalHandler::Disposition {
    int number;
    struct sigaction action;
};

SignalHandler::SignalHandler(const EventPipe &pipe)
    : SignalHandler(pipe, {SIGINT, SIGTERM}) {}

SignalHandler::SignalHandler(const EventPipe &pipe,
                             std::initializer_list<int> signals) {
    if (!pipe.reader_ || !pipe.writer_ || signals.size() == 0)
        throw std::invalid_argument(
            "SignalHandler requires an open pipe and signals.");
    for (auto signal = signals.begin(); signal != signals.end(); ++signal) {
        if (std::find(signals.begin(), signal, *signal) != signal)
            throw std::invalid_argument(
                "SignalHandler requires distinct signals.");
    }
    previous_.reserve(signals.size());
    struct sigaction action{};
    action.sa_handler = signalDispatcher;
    if (::sigemptyset(&action.sa_mask) < 0)
        throwSystemError("sigemptyset");

    int unused = -1;
    if (!signal_writer.compare_exchange_strong(unused, pipe.writer_.get()))
        throw std::logic_error("A SignalHandler is already active.");
    pipe_ = &pipe;
    stop_requested.store(false, std::memory_order_relaxed);
    try {
        for (const int signal : signals) {
            Disposition previous{signal, {}};
            if (::sigaction(signal, &action, &previous.action) < 0)
                throwSystemError("sigaction");
            previous_.push_back(previous);
        }
    } catch (...) {
        restore();
        throw;
    }
}

SignalHandler::~SignalHandler() noexcept { restore(); }

SignalHandler::SignalHandler(SignalHandler &&other) noexcept
    : previous_(std::move(other.previous_)),
      pipe_(std::exchange(other.pipe_, nullptr)) {}

SignalHandler &SignalHandler::operator=(SignalHandler &&other) noexcept {
    if (this != &other) {
        restore();
        previous_ = std::move(other.previous_);
        pipe_ = std::exchange(other.pipe_, nullptr);
    }
    return *this;
}

void SignalHandler::restore() noexcept {
    if (!pipe_)
        return;
    for (auto previous = previous_.rbegin(); previous != previous_.rend();
         ++previous) {
        retryInterrupted([&] {
            return ::sigaction(previous->number, &previous->action, nullptr);
        });
    }
    signal_writer.store(-1, std::memory_order_relaxed);
    pipe_ = nullptr;
    previous_.clear();
}

bool SignalHandler::isStopRequested() const noexcept {
    return pipe_ && stop_requested.load(std::memory_order_relaxed);
}

int SignalHandler::requestStop() const noexcept {
    return pipe_ ? requestStopAndNotify() : EBADF;
}

int SignalHandler::eventFd() const noexcept {
    return pipe_ ? pipe_->readerFd() : -1;
}

void SignalHandler::drain() const {
    if (!pipe_)
        throw std::logic_error("SignalHandler is inactive.");
    pipe_->drain();
}

} // namespace smart_home::platform
