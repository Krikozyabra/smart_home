#include "platform/DescriptorIO.h"
#include "platform/SystemError.h"

#include <unistd.h>

namespace smart_home::platform {

std::optional<std::size_t> readNonBlocking(int fd, void *buffer,
                                           std::size_t size) {
    const auto count =
        retryInterrupted([&] { return ::read(fd, buffer, size); });
    if (count >= 0)
        return static_cast<std::size_t>(count);
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return std::nullopt;
    throwSystemError("read");
}

} // namespace smart_home::platform
