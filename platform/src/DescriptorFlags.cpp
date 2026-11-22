#include "platform/DescriptorFlags.h"
#include "platform/SystemError.h"

#include <fcntl.h>

#include <stdexcept>
#include <utility>

namespace smart_home::platform {
namespace {

int getFlags(int fd) {
    if (fd < 0)
        throw std::invalid_argument("Expected a nonnegative descriptor.");
    const int flags = retryInterrupted([&] { return ::fcntl(fd, F_GETFL); });
    if (flags < 0)
        throwSystemError("fcntl(F_GETFL)");
    return flags;
}

int setFlags(int fd, int flags) noexcept {
    return retryInterrupted([&] { return ::fcntl(fd, F_SETFL, flags); });
}

} // namespace

NonBlockingScope::NonBlockingScope(int fd)
    : fd_(fd), saved_flags_(getFlags(fd)) {
    if (setFlags(fd_, saved_flags_ | O_NONBLOCK) < 0)
        throwSystemError("fcntl(F_SETFL)");
}

NonBlockingScope::~NonBlockingScope() noexcept { restoreFlags(); }

NonBlockingScope::NonBlockingScope(NonBlockingScope &&other) noexcept
    : fd_(std::exchange(other.fd_, -1)), saved_flags_(other.saved_flags_) {}

NonBlockingScope &
NonBlockingScope::operator=(NonBlockingScope &&other) noexcept {
    if (this != &other) {
        restoreFlags();
        fd_ = std::exchange(other.fd_, -1);
        saved_flags_ = other.saved_flags_;
    }
    return *this;
}

int NonBlockingScope::restoreFlags() noexcept {
    if (fd_ < 0)
        return 0;
    const int result = setFlags(fd_, saved_flags_);
    if (result == 0)
        fd_ = -1;
    return result;
}

void NonBlockingScope::restore() {
    if (restoreFlags() < 0)
        throwSystemError("fcntl(F_SETFL)");
}

} // namespace smart_home::platform
