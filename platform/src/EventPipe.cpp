#include "platform/EventPipe.h"
#include "EventPipeInternal.h"
#include "platform/DescriptorIO.h"
#include "platform/SystemError.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>

namespace smart_home::platform {

int detail::notifyEvent(int writer) noexcept {
    const int saved_errno = errno;
    const char byte = 1;
    const auto count = retryInterrupted(
        [&]() noexcept { return ::write(writer, &byte, sizeof(byte)); });
    const int error = count < 0 ? errno : 0;
    errno = saved_errno;
    return error == EAGAIN || error == EWOULDBLOCK ? 0 : error;
}

EventPipe::EventPipe() {
    int fds[2];
    if (retryInterrupted([&] { return ::pipe2(fds, O_NONBLOCK | O_CLOEXEC); }) <
        0)
        throwSystemError("pipe2");
    reader_ = FileDescriptor(fds[0]);
    writer_ = FileDescriptor(fds[1]);
}

int EventPipe::notify() const noexcept {
    return detail::notifyEvent(writer_.get());
}

void EventPipe::drain() const {
    char buffer[256];
    while (const auto count =
               readNonBlocking(readerFd(), buffer, sizeof(buffer))) {
        if (*count == 0)
            throw std::runtime_error("Event pipe unexpectedly closed.");
    }
}

} // namespace smart_home::platform
