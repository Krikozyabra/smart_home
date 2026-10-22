#pragma once
#include "Testing.h"
#include <filesystem>

namespace smart_home::ontology::detail {
class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}
    ~FileDescriptor();
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const noexcept { return fd_; }
    void reset(int fd = -1) noexcept;
private:
    int fd_;
};
std::filesystem::path canonicalDestination(const std::string& path);
int acquireLock(const std::filesystem::path& destination);
std::filesystem::path makeWorkspace(const std::filesystem::path& base, std::uintmax_t minimumFree);
void removeStaleSnapshots(const std::filesystem::path& destination);
void atomicSnapshot(const std::filesystem::path& source, const std::filesystem::path& destination,
                    const SnapshotHook& hook);
} // namespace smart_home::ontology::detail
