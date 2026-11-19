#include "persistence/StateLock.h"
#include "FileUtils.h"

#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace smart_home::persistence {
namespace {

int acquire(const std::filesystem::path &state) {
    detail::createStateDirectories(state);
    detail::FileDescriptor fd(detail::openPath(state / "runtime.lock",
                                               O_CREAT | O_RDWR | O_NONBLOCK));
    detail::requireRegularFile(fd.get());
    if (detail::retryInterrupted(
            [&] { return ::flock(fd.get(), LOCK_EX | LOCK_NB); }) < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            throw std::runtime_error(
                "Runtime state is locked by another process.");
        detail::systemError("flock runtime state");
    }
    detail::createStateDirectories(state / "generations");
    return fd.release();
}

} // namespace

StateLock::StateLock(const std::filesystem::path &state_dir)
    : directory_(detail::absolutePath(state_dir)), fd_(acquire(directory_)) {}

StateLock::~StateLock() noexcept { ::close(fd_); }

} // namespace smart_home::persistence
