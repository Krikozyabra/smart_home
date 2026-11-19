#pragma once

#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/types.h>

namespace smart_home::persistence::detail {

template <class Operation> auto retryInterrupted(Operation operation) {
    auto result = operation();
    while (result == -1 && errno == EINTR)
        result = operation();
    return result;
}

[[noreturn]] void systemError(const char *operation);

class FileDescriptor {
  public:
    explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
    ~FileDescriptor() noexcept;
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

    int get() const noexcept { return fd_; }
    int release() noexcept;
    void close();

  private:
    int fd_;
};

std::filesystem::path absolutePath(const std::filesystem::path &path);
int openPath(const std::filesystem::path &path, int flags, mode_t mode = 0600);
void requireRegularFile(int fd);
int openRegularFile(const std::filesystem::path &path);
ssize_t readBytes(int fd, void *data, std::size_t size);
void writeBytes(int fd, std::string_view bytes);
void syncFile(int fd);
void syncDirectory(const std::filesystem::path &directory);
void createStateDirectories(const std::filesystem::path &directory);
std::filesystem::path makeTemporaryDirectory(const std::filesystem::path &base,
                                             const char *prefix);
void renamePath(const std::filesystem::path &from,
                const std::filesystem::path &to);
void unlinkIfPresent(const std::filesystem::path &path);
std::string readSmallFile(const std::filesystem::path &path);
void writeSyncedFile(const std::filesystem::path &path,
                     std::string_view contents);
void removeTree(const std::filesystem::path &path) noexcept;

} // namespace smart_home::persistence::detail
