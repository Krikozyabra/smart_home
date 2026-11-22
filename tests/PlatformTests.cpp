#include "TestUtil.h"
#include "platform/DescriptorFlags.h"
#include "platform/DescriptorIO.h"
#include "platform/EventPipe.h"
#include "platform/FileDescriptor.h"
#include "platform/Poller.h"
#include "platform/SignalHandler.h"
#include "platform/SystemError.h"
#include "platform/TemporaryDirectory.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

namespace fs = std::filesystem;
using namespace smart_home::platform;

struct Pipe {
    FileDescriptor reader;
    FileDescriptor writer;

    Pipe() {
        int fds[2];
        if (::pipe(fds) < 0)
            throwSystemError("pipe");
        reader.reset(fds[0]);
        writer.reset(fds[1]);
    }
};

bool isClosed(int fd) { return ::fcntl(fd, F_GETFD) == -1 && errno == EBADF; }

void testFileDescriptor() {
    FileDescriptor fd;
    require(!fd && !fd.valid() && fd.get() == -1,
            "Expected an empty descriptor");

    Pipe pipe;
    const int reader = pipe.reader.get();
    const int writer = pipe.writer.get();
    FileDescriptor moved(std::move(pipe.reader));
    require(moved.get() == reader && !pipe.reader,
            "Move must transfer ownership");
    fd = std::move(moved);
    require(fd.valid() && fd.get() == reader && !moved,
            "Move assignment must transfer ownership");
    fd.reset(reader);
    require(!isClosed(reader),
            "Reset to the same descriptor must leave it open");
    fd.reset(pipe.writer.release());
    require(
        isClosed(reader) && fd.get() == writer && !pipe.writer,
        "Reset must close the previous descriptor and adopt the released one");
    fd.close();
    fd.close();
    require(!fd && isClosed(writer),
            "Explicit close must leave the descriptor empty");

    int during_exception = -1;
    expectException<std::runtime_error>([&] {
        Pipe owned;
        during_exception = owned.reader.get();
        throw std::runtime_error("Unwind through descriptor owner");
    });
    require(during_exception >= 0 && isClosed(during_exception),
            "Unwinding must close the owned descriptor");
}

void testEventPipe() {
    EventPipe original;
    EventPipe pipe(std::move(original));
    const int reader = pipe.readerFd();
    const int flags = ::fcntl(reader, F_GETFL);
    const int descriptor_flags = ::fcntl(reader, F_GETFD);
    require(flags >= 0 && (flags & O_NONBLOCK),
            "Event pipe must be nonblocking");
    require(descriptor_flags >= 0 && (descriptor_flags & FD_CLOEXEC),
            "Event pipe must be close-on-exec");
    errno = EDOM;
    const int moved_error = original.notify();
    const int preserved_errno = errno;
    require(moved_error == EBADF && preserved_errno == EDOM,
            "Notification must report its own error and preserve errno");

    Poller poller;
    poller.add(reader, PollEvent::Read);
    require(poller.poll(0) == 0, "A new pipe must be empty");
    auto writer = std::async(std::launch::async, [&] { return pipe.notify(); });
    const int ready = poller.poll(2000);
    require(writer.get() == 0 && ready == 1 && poller.isReadable(reader),
            "Notification from another thread must wake the poller");

    const int capacity = ::fcntl(reader, F_GETPIPE_SZ);
    require(capacity > 0, "Expected a positive pipe capacity");
    for (int i = 0; i <= capacity; ++i)
        require(pipe.notify() == 0, "A full pipe already holds a wakeup");
    pipe.drain();
    pipe.drain();
    require(poller.poll(0) == 0 && !poller.isReadable(reader),
            "Drain must empty the pipe");
    require(pipe.notify() == 0 && poller.poll(0) == 1,
            "A drained pipe can wake again");
}

volatile sig_atomic_t last_signal = 0;

extern "C" {
static void recordSignal(int number) noexcept { last_signal = number; }
}

class ScopedAction {
  public:
    explicit ScopedAction(int number) : number_(number) {
        struct sigaction action{};
        action.sa_handler = recordSignal;
        ::sigemptyset(&action.sa_mask);
        if (::sigaction(number_, &action, &previous_) < 0)
            throwSystemError("sigaction");
    }
    ~ScopedAction() { ::sigaction(number_, &previous_, nullptr); }

  private:
    int number_;
    struct sigaction previous_{};
};

void testSignalHandler() {
    ScopedAction old_interrupt(SIGINT);
    ScopedAction old_terminate(SIGTERM);
    EventPipe pipe;
    expectException<std::system_error>(
        [&] { SignalHandler invalid(pipe, {SIGINT, SIGKILL}); });
    last_signal = 0;
    require(::raise(SIGINT) == 0 && last_signal == SIGINT,
            "Partial registration must restore the previous action");

    for (const int number : {SIGINT, SIGTERM}) {
        {
            SignalHandler original(pipe);
            SignalHandler signals(std::move(original));
            require(!original.isStopRequested() && original.eventFd() == -1 &&
                        !signals.isStopRequested(),
                    "Moved handler must own a fresh stop flag");
            Poller poller;
            poller.add(signals.eventFd(), PollEvent::Read);
            last_signal = 0;
            errno = ERANGE;
            const int raised = ::raise(number);
            const int preserved_errno = errno;
            require(
                raised == 0 && preserved_errno == ERANGE && last_signal == 0,
                "Installed handler must catch the signal and preserve errno");
            expectException<std::logic_error>(
                [&] { SignalHandler duplicate(pipe); });
            require(signals.isStopRequested() && poller.poll(0) == 1 &&
                        poller.isReadable(signals.eventFd()),
                    "Signal must request stop and wake the pipe");
            signals.drain();
            require(poller.poll(0) == 0 && signals.isStopRequested(),
                    "Draining the pipe must preserve the stop flag");
        }
        require(::raise(number) == 0 && last_signal == number,
                "The previous action must be restored");
    }

    SignalHandler signals(pipe, {SIGTERM});
    last_signal = 0;
    require(::raise(SIGINT) == 0 && last_signal == SIGINT &&
                !signals.isStopRequested(),
            "Custom registration must affect only listed signals");
    require(signals.requestStop() == 0 && signals.isStopRequested(),
            "Programmatic stop must set the flag");
    Poller poller;
    poller.add(signals.eventFd(), PollEvent::Read);
    require(poller.poll(0) == 1, "Programmatic stop must also wake the pipe");
}

void testDescriptorFlagsAndRead() {
    Pipe pipe;
    const int fd = pipe.reader.get();
    const int flags = ::fcntl(fd, F_GETFL);
    require(flags >= 0 && !(flags & O_NONBLOCK),
            "Expected a blocking test descriptor");
    expectException<std::invalid_argument>(
        [] { NonBlockingScope invalid(-1); });
    {
        NonBlockingScope nonblocking(fd);
        NonBlockingScope moved(std::move(nonblocking));
        require(::fcntl(fd, F_GETFL) == (flags | O_NONBLOCK),
                "Scope must set O_NONBLOCK");
        char byte = 0;
        require(!readNonBlocking(fd, &byte, 1),
                "Empty nonblocking pipe must return nullopt");
        require(::write(pipe.writer.get(), "x", 1) == 1,
                "Could not write the test byte");
        require(readNonBlocking(fd, &byte, 1) == 1 && byte == 'x',
                "Read must return available data");
        pipe.writer.close();
        require(readNonBlocking(fd, &byte, 1) == 0,
                "EOF must be distinct from EAGAIN");
    }
    require(::fcntl(fd, F_GETFL) == flags,
            "Scope must restore all status flags");
    NonBlockingScope explicit_restore(fd);
    explicit_restore.restore();
    explicit_restore.restore();
    require(::fcntl(fd, F_GETFL) == flags,
            "Explicit restoration must be repeatable");
}

void testTemporaryDirectory() {
    TemporaryDirectory base(fs::temp_directory_path(),
                            "smart-home-platform-tests-");
    expectException<std::invalid_argument>(
        [] { TemporaryDirectory invalid({}, "test-"); });
    expectException<std::invalid_argument>(
        [&] { TemporaryDirectory invalid(base.path(), "../escape-"); });
    expectException<std::system_error>(
        [&] { TemporaryDirectory missing(base.path() / "missing", "test-"); });

    fs::path removed;
    fs::path preserved;
    {
        TemporaryDirectory normal(base.path(), "test-");
        TemporaryDirectory kept(base.path(), "test-");
        removed = normal.path();
        preserved = kept.path();
        require(
            removed != preserved && removed.parent_path() == base.path() &&
                removed.filename().string().rfind("test-", 0) == 0,
            "Temporary directories must have unique names under the base path");
        fs::create_directory(removed / "nested");
        std::ofstream(removed / "nested" / "file").put('x');
        require(fs::exists(removed / "nested" / "file"),
                "Could not create a test file");
        kept.preserve();
        TemporaryDirectory moved(std::move(kept));
        require(kept.path().empty() && moved.path() == preserved,
                "Move must transfer the directory");
        normal.preserve();
        normal.setPreserve(false);
    }
    require(!fs::exists(removed),
            "Cleanup must remove the nested contents and directory");
    require(fs::is_directory(preserved),
            "Preserved directory must survive destruction after a move");
}

void testPoller() {
    Pipe first;
    Pipe second;
    const int first_reader = first.reader.get();
    const int first_writer = first.writer.get();
    const int second_reader = second.reader.get();
    const int second_writer = second.writer.get();
    Poller poller;
    poller.add(first_reader, PollEvent::Read | PollEvent::Write);
    poller.add(first_writer, PollEvent::Write);
    poller.add(second_reader, PollEvent::Read);
    poller.add(second_writer, PollEvent::Write);
    expectException<std::invalid_argument>(
        [&] { poller.add(first_reader, PollEvent::Read); });
    require(poller.poll(0) == 2 && poller.isWritable(first_writer) &&
                poller.isWritable(second_writer) &&
                !poller.isReadable(first_reader),
            "Only writers should be ready on empty pipes");
    require(::write(first_writer, "x", 1) == 1 &&
                ::write(second_writer, "x", 1) == 1,
            "Could not make both test pipes readable");
    require(poller.poll(0) == 4 && poller.isReadable(first_reader) &&
                poller.isReadable(second_reader),
            "Poll must report both readable descriptors");

    first.writer.close();
    second.reader.close();
    require(
        poller.poll(0) == 4 && poller.isHangUp(first_reader) &&
            poller.isReadable(first_reader) && poller.hasError(second_writer) &&
            poller.isInvalid(first_writer) && poller.isInvalid(second_reader),
        "Poll must distinguish pending data, hangups, errors and invalid "
        "descriptors");
}

class PeriodicAlarm {
  public:
    PeriodicAlarm() {
        struct itimerval timer{};
        timer.it_value.tv_usec = 20'000;
        timer.it_interval = timer.it_value;
        if (::setitimer(ITIMER_REAL, &timer, &previous_) < 0)
            throwSystemError("setitimer");
    }
    ~PeriodicAlarm() { ::setitimer(ITIMER_REAL, &previous_, nullptr); }

  private:
    struct itimerval previous_{};
};

void testInterruptedPoll() {
    using namespace std::chrono;
    ScopedAction action(SIGALRM);
    EventPipe pipe;
    Poller poller;
    poller.add(pipe.readerFd(), PollEvent::Read);
    last_signal = 0;
    PeriodicAlarm alarm;
    const auto start = steady_clock::now();
    require(poller.poll(100) == 0 &&
                steady_clock::now() - start >= milliseconds(100) &&
                last_signal == SIGALRM,
            "Repeated interruptions must preserve the total timeout");
}

void testSystemError() {
    int attempts = 0;
    const auto result = retryInterrupted([&] {
        if (++attempts <= 2) {
            errno = EINTR;
            return -1;
        }
        errno = EBADF;
        return -1;
    });
    const int final_errno = errno;
    require(result == -1 && attempts == 3 && final_errno == EBADF,
            "Only EINTR should be retried");
    errno = EBADF;
    try {
        throwSystemError("test operation");
    } catch (const std::system_error &error) {
        const std::string_view message = error.what();
        require(
            error.code() == std::error_code(EBADF, std::generic_category()) &&
                message.find("test operation") != std::string_view::npos &&
                message.find(error.code().message()) != std::string_view::npos,
            "System error must include the operation and errno description");
    }
}

} // namespace

int main() {
    try {
        testFileDescriptor();
        testEventPipe();
        testSignalHandler();
        testDescriptorFlagsAndRead();
        testTemporaryDirectory();
        testPoller();
        testInterruptedPoll();
        testSystemError();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
