#include "FileUtils.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

namespace smart_home::persistence::detail {

[[noreturn]] void systemError(const char *operation) {
    const int error = errno;
    throw std::system_error(error, std::generic_category(), operation);
}

FileDescriptor::~FileDescriptor() noexcept {
    if (fd_ != -1)
        ::close(fd_);
}

int FileDescriptor::release() noexcept { return std::exchange(fd_, -1); }

void FileDescriptor::close() {
    // Linux has already released the descriptor even if close returns EINTR.
    if (::close(release()) < 0)
        systemError("close runtime file");
}

fs::path absolutePath(const fs::path &path) {
    if (path.empty() || path.native().find('\0') != std::string::npos)
        throw std::invalid_argument(
            "Runtime path must be nonempty and without NUL.");
    return fs::absolute(path);
}

int openPath(const fs::path &path, int flags, mode_t mode) {
    const int fd = retryInterrupted([&] {
        return ::open(path.c_str(), flags | O_CLOEXEC | O_NOFOLLOW, mode);
    });
    if (fd < 0)
        systemError("open runtime file");
    return fd;
}

void requireRegularFile(int fd) {
    struct stat info{};
    if (retryInterrupted([&] { return ::fstat(fd, &info); }) < 0)
        systemError("fstat runtime file");
    if (!S_ISREG(info.st_mode))
        throw std::runtime_error("Expected a regular runtime file.");
}

int openRegularFile(const fs::path &path) {
    FileDescriptor file(openPath(path, O_RDONLY | O_NONBLOCK));
    requireRegularFile(file.get());
    return file.release();
}

ssize_t readBytes(int fd, void *data, std::size_t size) {
    const auto count = retryInterrupted([&] { return ::read(fd, data, size); });
    if (count < 0)
        systemError("read runtime file");
    return count;
}

void writeBytes(int fd, std::string_view bytes) {
    while (!bytes.empty()) {
        const auto count = retryInterrupted(
            [&] { return ::write(fd, bytes.data(), bytes.size()); });
        if (count < 0)
            systemError("write runtime file");
        if (count == 0)
            throw std::runtime_error("Short write to runtime file.");
        bytes.remove_prefix(static_cast<std::size_t>(count));
    }
}

void syncFile(int fd) {
    if (retryInterrupted([&] { return ::fsync(fd); }) < 0)
        systemError("fsync runtime file");
}

void syncDirectory(const fs::path &directory) {
    const FileDescriptor fd(openPath(directory, O_RDONLY | O_DIRECTORY));
    syncFile(fd.get());
}

void createStateDirectories(const fs::path &directory) {
    const auto status = fs::symlink_status(directory);
    if (fs::is_directory(status))
        return;
    if (fs::exists(status))
        throw std::runtime_error("Expected a runtime directory.");
    const auto parent = directory.parent_path();
    createStateDirectories(parent);
    if (retryInterrupted([&] { return ::mkdir(directory.c_str(), 0700); }) <
            0 &&
        (errno != EEXIST || !fs::is_directory(fs::symlink_status(directory))))
        systemError("mkdir runtime directory");
    syncDirectory(directory);
    syncDirectory(parent);
}

fs::path makeTemporaryDirectory(const fs::path &base, const char *prefix) {
    auto pattern = (base / (std::string(prefix) + "XXXXXX")).string();
    const auto offset = pattern.size() - 6;
    for (;;) {
        if (::mkdtemp(pattern.data()))
            break;
        if (errno != EINTR)
            systemError("mkdtemp runtime directory");
        pattern.replace(offset, 6, "XXXXXX");
    }
    try {
        return fs::path(pattern);
    } catch (...) {
        retryInterrupted([&] { return ::rmdir(pattern.c_str()); });
        throw;
    }
}

void renamePath(const fs::path &from, const fs::path &to) {
    if (retryInterrupted([&] { return ::rename(from.c_str(), to.c_str()); }) <
        0)
        systemError("rename runtime path");
}

void unlinkIfPresent(const fs::path &path) {
    if (retryInterrupted([&] { return ::unlink(path.c_str()); }) < 0 &&
        errno != ENOENT)
        systemError("unlink runtime file");
}

std::string readSmallFile(const fs::path &path) {
    const FileDescriptor file(openRegularFile(path));
    std::string result;
    std::array<char, 1024> bytes;
    while (const auto count =
               readBytes(file.get(), bytes.data(), bytes.size())) {
        result.append(bytes.data(), static_cast<std::size_t>(count));
        if (result.size() > 4096)
            throw std::runtime_error("Invalid runtime manifest file size.");
    }
    return result;
}

void writeSyncedFile(const fs::path &path, std::string_view contents) {
    FileDescriptor file(openPath(path, O_WRONLY | O_CREAT | O_EXCL));
    writeBytes(file.get(), contents);
    syncFile(file.get());
    file.close();
}

void removeTree(const fs::path &path) noexcept {
    try {
        std::error_code error;
        do {
            fs::remove_all(path, error);
        } while (error == std::errc::interrupted);
    } catch (...) {
        // Cleanup must not replace the original error.
    }
}

} // namespace smart_home::persistence::detail
