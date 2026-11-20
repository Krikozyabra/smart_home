#include "platform/FileDescriptor.h"
#include "platform/SystemError.h"

#include <unistd.h>

#include <utility>

namespace smart_home::platform {

FileDescriptor::FileDescriptor(int fd) noexcept : fd_(fd < 0 ? -1 : fd) {}

FileDescriptor::~FileDescriptor() noexcept {
    if (valid()) {
        const int saved_errno = errno;
        ::close(fd_);
        errno = saved_errno;
    }
}

FileDescriptor::FileDescriptor(FileDescriptor &&other) noexcept
    : fd_(other.release()) {}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&other) noexcept {
    if (this != &other) {
        const FileDescriptor previous(release());
        fd_ = other.release();
    }
    return *this;
}

int FileDescriptor::release() noexcept { return std::exchange(fd_, -1); }

void FileDescriptor::reset(int fd) {
    if (fd < 0)
        fd = -1;
    if (fd == fd_)
        return;
    const int previous = std::exchange(fd_, fd);
    if (previous >= 0 && ::close(previous) < 0)
        throwSystemError("close");
}

void FileDescriptor::close() { reset(); }

} // namespace smart_home::platform
