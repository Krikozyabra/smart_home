#include "TestUtil.h"
#include "events/EventBus.h"
#include "events/Reactor.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace smart_home::events;
using namespace std::chrono_literals;

struct Sample {
    int value;
};

struct OtherEvent {};

class Pipe {
  public:
    Pipe() {
        if (::pipe2(fds_, O_NONBLOCK | O_CLOEXEC) < 0)
            throw std::system_error(errno, std::generic_category(), "pipe2");
    }
    ~Pipe() {
        closeRead();
        closeWrite();
    }
    Pipe(const Pipe &) = delete;
    Pipe &operator=(const Pipe &) = delete;

    int reader() const noexcept { return fds_[0]; }
    int writer() const noexcept { return fds_[1]; }
    void closeRead() noexcept { close(fds_[0]); }
    void closeWrite() noexcept { close(fds_[1]); }
    void notify() const {
        require(::write(writer(), "x", 1) == 1, "Write test byte");
    }
    bool consume() const {
        char byte = 0;
        return ::read(reader(), &byte, sizeof(byte)) == 1 && byte == 'x';
    }

  private:
    static void close(int &fd) noexcept {
        if (fd >= 0)
            ::close(std::exchange(fd, -1));
    }
    int fds_[2];
};

void testSubscription() {
    EventBus bus;
    std::vector<int> received;
    auto first = bus.subscribe<Sample>(
        [&](const Sample &event) { received.push_back(event.value); });
    auto second = bus.subscribe<Sample>([&, count = 0](const Sample &) mutable {
        received.push_back(++count);
    });
    const auto unrelated = bus.subscribe<OtherEvent>(
        [&](const OtherEvent &) { received.push_back(-1); });
    bus.publish(Sample{10});
    bus.publish(Sample{20});
    bus.publish(OtherEvent{});
    require(received == std::vector<int>({10, 1, 20, 2, -1}),
            "Delivery must keep FIFO, handler state and type isolation");
    require(bus.subscriberCount() == 3, "Expected three active subscriptions");

    {
        Subscription moved = std::move(first);
        require(!first.isSubscribed() && moved.isSubscribed(),
                "Move must transfer a subscription");
        moved = std::move(second);
        require(bus.subscriberCount() == 2 && !second.isSubscribed(),
                "Move assignment must cancel the previous registration");
        bus.publish(Sample{30});
        require(received.back() == 3, "Only the moved handler should remain");
    }
    require(bus.subscriberCount() == 1,
            "Token destruction must unregister its handler");
    bus.publish(Sample{40});
    require(received.size() == 6, "Destroyed tokens must prevent delivery");
    expectException<std::invalid_argument>([&] { bus.subscribe<Sample>({}); });

    Subscription detached;
    {
        EventBus temporary;
        detached = temporary.subscribe<Sample>([](const Sample &) {});
    }
    require(!detached.isSubscribed(), "Expired bus must invalidate its tokens");
    detached.cancel();
}

void testReentrantPublication() {
    EventBus bus;
    Subscription cancelled;
    Subscription added;
    std::vector<int> received;
    const auto first = bus.subscribe<Sample>([&](const Sample &event) {
        received.push_back(10 + event.value);
        if (event.value == 1) {
            cancelled.cancel();
            added = bus.subscribe<Sample>([&](const Sample &nested) {
                received.push_back(40 + nested.value);
            });
            bus.publish(Sample{2});
        }
    });
    cancelled = bus.subscribe<Sample>(
        [&](const Sample &event) { received.push_back(20 + event.value); });
    const auto last = bus.subscribe<Sample>(
        [&](const Sample &event) { received.push_back(30 + event.value); });
    bus.publish(Sample{1});
    require(received == std::vector<int>({11, 12, 32, 42, 31}),
            "Nested publication must see new registrations and skip cancelled "
            "callbacks");
    cancelled.cancel();
    require(!cancelled.isSubscribed() && bus.subscriberCount() == 3,
            "Cancellation must be idempotent");
}

void testConcurrentSubscription() {
    EventBus bus;
    std::promise<void> entered;
    std::promise<void> resume;
    auto entered_future = entered.get_future();
    auto resume_future = resume.get_future();
    const auto first = bus.subscribe<Sample>([&](const Sample &) {
        entered.set_value();
        resume_future.wait();
    });
    int cancelled_calls = 0;
    int added_calls = 0;
    auto cancelled =
        bus.subscribe<Sample>([&](const Sample &) { ++cancelled_calls; });
    auto publisher =
        std::async(std::launch::async, [&] { bus.publish(Sample{1}); });
    entered_future.wait();
    cancelled.cancel();
    const auto added =
        bus.subscribe<Sample>([&](const Sample &) { ++added_calls; });
    resume.set_value();
    publisher.get();
    require(cancelled_calls == 0 && added_calls == 0 &&
                bus.subscriberCount() == 2,
            "Concurrent changes must skip removed callbacks and keep the "
            "publication snapshot");
}

void testPublicationFailure() {
    EventBus bus;
    auto failing = bus.subscribe<Sample>(
        [](const Sample &) { throw std::runtime_error("subscriber"); });
    int visited = 0;
    const auto healthy =
        bus.subscribe<Sample>([&](const Sample &) { ++visited; });
    expectException<std::runtime_error>([&] { bus.publish(Sample{1}); });
    require(visited == 1,
            "A throwing subscriber must allow later subscribers to run");
    failing.cancel();
    bus.publish(Sample{2});
    require(visited == 2, "A previous exception must leave the bus usable");
}

void testDescriptors() {
    Pipe pipe;
    Reactor reactor;
    IoEvent input = IoEvent::None;
    IoEvent output = IoEvent::None;
    reactor.addHandler(pipe.reader(), IoEvent::Read,
                       [&](int fd, IoEvent events) {
                           if (fd == pipe.reader())
                               input = events;
                       });
    reactor.addHandler(pipe.writer(), IoEvent::Write,
                       [&](int, IoEvent events) { output = events; });
    require(reactor.step(0) == 1 && output == IoEvent::Write &&
                input == IoEvent::None,
            "An empty pipe must report only write readiness");
    reactor.modifyHandler(pipe.writer(), IoEvent::None);
    pipe.notify();
    require(reactor.step(0) == 1 && input == IoEvent::Read,
            "Expected only readable input");
    reactor.removeHandler(pipe.writer());
    pipe.closeWrite();
    require(reactor.step(0) == 1 && input == (IoEvent::Read | IoEvent::HangUp),
            "Hangup must be reported alongside unread data");
    require(pipe.consume(), "The reactor must borrow both descriptors");
    reactor.removeHandler(pipe.reader());
    require(reactor.step(0) == 0, "Removed descriptor must be ignored");

    Pipe disconnected;
    disconnected.closeRead();
    reactor.addHandler(disconnected.writer(), IoEvent::None,
                       [&](int, IoEvent events) { output = events; });
    require(reactor.step(0) == 1 && output == IoEvent::Error,
            "Broken writes must report errors even without a requested mask");
    reactor.removeHandler(disconnected.writer());
    expectException<std::invalid_argument>(
        [&] { reactor.addHandler(-1, IoEvent::Read, [](int, IoEvent) {}); });
    expectException<std::invalid_argument>(
        [&] { reactor.modifyHandler(-1, IoEvent::Read); });
}

void testRegistrationChanges() {
    Pipe first;
    Pipe second;
    Reactor reactor;
    bool replaced_called = false;
    bool replacement_called = false;
    reactor.addHandler(first.reader(), IoEvent::Read, [&](int fd, IoEvent) {
        reactor.removeHandler(fd);
        reactor.removeHandler(second.reader());
        reactor.addHandler(second.reader(), IoEvent::Read, [&](int, IoEvent) {
            replacement_called = second.consume();
        });
    });
    reactor.addHandler(second.reader(), IoEvent::Read,
                       [&](int, IoEvent) { replaced_called = true; });
    first.notify();
    second.notify();
    require(reactor.step(0) == 1 && !replaced_called && !replacement_called,
            "Removed and replacement callbacks must skip readiness from the "
            "current poll");
    require(reactor.step(0) == 1 && replacement_called,
            "Next poll must call the replacement handler");
}

void testTimers() {
    Reactor reactor;
    int called = 0;
    TimerHandle second;
    auto first = reactor.addTimer(0ms, false, [&] {
        second.cancel();
        ++called;
    });
    second = reactor.addTimer(0ms, false, [&] { called += 100; });
    {
        const auto scoped =
            reactor.addTimer(0ms, false, [&] { called += 100; });
    }
    require(reactor.step(0) == 1 && called == 1 && !first.isActive() &&
                !second.isActive(),
            "Cancellation and token destruction must skip due timers");
    require(reactor.step(0) == 0,
            "A one-shot timer must expire before its callback");

    TimerHandle periodic;
    const auto start = std::chrono::steady_clock::now();
    periodic = reactor.addTimer(5ms, true, [&] {
        if (++called == 3)
            periodic.cancel();
    });
    TimerHandle moved(std::move(periodic));
    periodic = std::move(moved);
    require(reactor.step(500) == 1 && called == 2 && periodic.isActive() &&
                std::chrono::steady_clock::now() - start >= 5ms,
            "Poll must wait for the monotonic timer deadline");
    require(reactor.step(500) == 1 && called == 3 && !periodic.isActive(),
            "A periodic timer can cancel itself during callback");
    require(reactor.step(0) == 0,
            "A cancelled periodic timer must not fire again");
    expectException<std::invalid_argument>(
        [&] { reactor.addTimer(0ms, true, [] {}); });
    expectException<std::invalid_argument>(
        [&] { reactor.addTimer(-1ms, false, [] {}); });
    expectException<std::invalid_argument>([&] {
        reactor.addTimer(std::chrono::milliseconds::max(), false, [] {});
    });
}

void testThreadedPostAndStop() {
    Reactor reactor;
    EventBus bus;
    const auto owner = std::this_thread::get_id();
    bool same_thread = true;
    bool running = false;
    std::vector<int> values;
    const auto subscription =
        bus.subscribe<std::unique_ptr<int>>([&](const auto &value) {
            same_thread &= std::this_thread::get_id() == owner;
            values.push_back(*value);
        });
    std::promise<void> started;
    auto gate = started.get_future().share();
    std::promise<void> drained;
    auto completion = drained.get_future();
    auto producers = std::async(std::launch::async, [&] {
        gate.wait();
        auto another = std::async(std::launch::async, [&] {
            reactor.post(bus, std::make_unique<int>(3));
        });
        reactor.post(bus, std::make_unique<int>(1));
        reactor.post(bus, std::make_unique<int>(2));
        another.get();
        reactor.post([&] { drained.set_value(); });
        completion.wait();
        reactor.stop();
    });
    reactor.post([&] {
        running = reactor.isRunning();
        started.set_value();
    });
    reactor.run();
    producers.get();
    require(running && !reactor.isRunning() && same_thread,
            "All callbacks must run on the loop thread");
    require(
        values == std::vector<int>({1, 2, 3}) ||
            values == std::vector<int>({1, 3, 2}) ||
            values == std::vector<int>({3, 1, 2}),
        "Every typed event must be delivered in FIFO order for each producer");
}

void testStepAndStop() {
    Reactor reactor;
    int calls = 0;
    reactor.post([&] { ++calls; });
    reactor.stop();
    require(reactor.step(0) == 0 && calls == 0,
            "A stop requested before step must take effect");
    require(reactor.step(0) == 1 && calls == 1,
            "A stopped step must preserve queued work");
    reactor.post([&] { reactor.stop(); });
    reactor.post([&] { ++calls; });
    require(reactor.step(0) == 1 && calls == 1,
            "A stop callback must prevent further dispatch in its batch");
    require(reactor.step(0) == 1 && calls == 2,
            "Queued work must survive an in-flight stop");
    reactor.post([&] { reactor.post([&] { ++calls; }); });
    require(reactor.step(0) == 1 && calls == 2,
            "A task posted from a callback belongs to the next step");
    require(reactor.step(0) == 1 && calls == 3,
            "The deferred task must still be delivered");

    Subscription temporary;
    {
        EventBus bus;
        temporary = bus.subscribe<Sample>([&](const Sample &) { ++calls; });
        reactor.post(bus, Sample{1});
    }
    require(reactor.step(0) == 1 && calls == 3 && !temporary.isSubscribed(),
            "Queued event must skip delivery to a destroyed bus");
}

void testReactorFailures() {
    Pipe failed_io;
    Pipe healthy_io;
    std::vector<std::string> errors;
    Reactor reactor([&](std::exception_ptr error) {
        try {
            std::rethrow_exception(error);
        } catch (const std::exception &exception) {
            errors.emplace_back(exception.what());
        }
        throw std::runtime_error("error reporter");
    });
    int survived = 0;
    bool rejected_recursion = false;
    reactor.addHandler(failed_io.reader(), IoEvent::Read, [&](int, IoEvent) {
        failed_io.consume();
        throw std::runtime_error("I/O");
    });
    reactor.addHandler(healthy_io.reader(), IoEvent::Read,
                       [&](int, IoEvent) { survived += healthy_io.consume(); });
    failed_io.notify();
    healthy_io.notify();
    reactor.post([] { throw std::runtime_error("task"); });
    reactor.post([&] {
        try {
            reactor.step(0);
        } catch (const std::logic_error &) {
            rejected_recursion = true;
        }
        ++survived;
    });
    const auto faulty =
        reactor.addTimer(0ms, false, [] { throw std::runtime_error("timer"); });
    const auto healthy = reactor.addTimer(0ms, false, [&] { ++survived; });
    require(reactor.step(0) == 6 && survived == 3 && rejected_recursion &&
                errors == std::vector<std::string>({"I/O", "task", "timer"}),
            "Exceptions must reach the receiver without interrupting the "
            "dispatch batch");
    require(reactor.step(0) == 0 && !faulty.isActive() && !healthy.isActive(),
            "Failed callbacks must leave the reactor and timers reusable");
}

} // namespace

int main() {
    try {
        testSubscription();
        testReentrantPublication();
        testConcurrentSubscription();
        testPublicationFailure();
        testDescriptors();
        testRegistrationChanges();
        testTimers();
        testThreadedPostAndStop();
        testStepAndStop();
        testReactorFailures();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
