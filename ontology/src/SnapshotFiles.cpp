#include "SnapshotFiles.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace smart_home::ontology::detail {
namespace fs = std::filesystem;
namespace {
[[noreturn]] void ioError(const char* action) {
    throw std::runtime_error(std::string("Ontology ") + action + ": " + std::strerror(errno));
}
void syncFd(int fd, const char* description) {
    int result;
    do { result = ::fsync(fd); } while (result < 0 && errno == EINTR);
    if (result < 0) ioError(description);
}
void validateRegularFile(int fd) {
    struct stat info{};
    if (::fstat(fd, &info) < 0) ioError("stat file");
    if (!S_ISREG(info.st_mode) || info.st_nlink != 1)
        throw std::runtime_error("Ontology database and lock must be regular files without hard-link aliases.");
}
} // namespace

FileDescriptor::~FileDescriptor() { reset(); }
void FileDescriptor::reset(int fd) noexcept {
    if (fd_ >= 0) ::close(fd_);
    fd_ = fd;
}

fs::path canonicalDestination(const std::string& path) {
    const auto absolute = fs::absolute(path);
    fs::create_directories(absolute.parent_path());
    const auto canonical = fs::weakly_canonical(absolute);
    if (canonical.filename().empty() || (fs::exists(canonical) && !fs::is_regular_file(canonical)))
        throw std::invalid_argument("Ontology destination must be a regular database file.");
    if (fs::exists(canonical) && fs::hard_link_count(canonical) != 1)
        throw std::runtime_error("Hard-linked ontology databases are unsupported.");
    return canonical;
}

fs::path validateDumpDestination(const std::string& path, const fs::path& runtime,
                                 const fs::path& database) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::invalid_argument("Ontology dump path must be nonempty and contain no NUL.");
    const auto absolute = fs::absolute(path);
    const auto status = fs::symlink_status(absolute);
    if (fs::is_symlink(status) || (fs::exists(status) && !fs::is_regular_file(status)))
        throw std::invalid_argument("Ontology dump destination must be a regular file, not a link or directory.");
    if (!fs::is_directory(absolute.parent_path()))
        throw std::invalid_argument("Ontology dump parent directory must exist.");
    const auto destination = fs::canonical(absolute.parent_path()) / absolute.filename();
    const auto workspace = fs::canonical(runtime.parent_path());
    const auto activeLock = fs::path(database.string() + ".lock");
    // Reject the entire workspace, including engine sidecars, and inode aliases.
    const auto relative = destination.lexically_relative(workspace);
    if ((!relative.empty() && *relative.begin() != "..") || destination == activeLock ||
        (fs::exists(destination) &&
         ((fs::exists(runtime) && fs::equivalent(destination, runtime)) ||
          fs::equivalent(destination, activeLock))))
        throw std::invalid_argument("Ontology dump cannot replace its workspace or active lockfile.");
    if (fs::exists(status) && fs::hard_link_count(absolute) != 1)
        throw std::invalid_argument("Hard-linked ontology dump destinations are unsupported.");
    return destination;
}

int acquireLock(const fs::path& destination) {
    const auto path = destination.string() + ".lock";
    const int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) ioError("open lock");
    try {
        validateRegularFile(fd);
        if (::flock(fd, LOCK_EX | LOCK_NB) < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                throw std::runtime_error("Ontology database is already in use by another process.");
            ioError("lock database");
        }
    } catch (...) { ::close(fd); throw; }
    return fd; // Never unlink a lockfile: its inode must outlive atomic snapshots.
}

fs::path makeWorkspace(const fs::path& base, std::uintmax_t minimumFree) {
    if (fs::space(base).available < minimumFree)
        throw std::runtime_error("Insufficient free space for ontology workspace.");
    std::string pattern = (base / "smart_home_ontology_XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    const auto* directory = ::mkdtemp(buffer.data()); // Atomic creation, mode 0700.
    if (!directory) ioError("create private workspace");
    return fs::path(directory);
}

void removeStaleSnapshots(const fs::path& destination) {
    const auto prefix = "." + destination.filename().string() + ".tmp_";
    for (const auto& entry : fs::directory_iterator(destination.parent_path())) {
        const auto name = entry.path().filename().string();
        if (name.compare(0, prefix.size(), prefix) != 0) continue;
        // Our mkstemp suffix is precisely six ASCII alphanumerics. Leave unrelated
        // names, directories and symlinks alone, even if they share the prefix.
        const auto suffix = name.substr(prefix.size());
        if (suffix.size() != 6 || suffix.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
            continue;
        if (fs::is_regular_file(entry.symlink_status())) fs::remove(entry.path());
    }
}

void atomicSnapshot(const fs::path& source, const fs::path& destination, const SnapshotHook& hook) {
    FileDescriptor input(::open(source.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (input.get() < 0) ioError("open snapshot source");
    validateRegularFile(input.get());
    std::string pattern = (destination.parent_path() /
        ("." + destination.filename().string() + ".tmp_XXXXXX")).string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    FileDescriptor output(::mkstemp(name.data()));
    if (output.get() < 0) ioError("create snapshot");
    const fs::path temporary(name.data());
    bool renamed = false;
    try {
        if (hook) hook(SnapshotStage::BeforeCopy);
        std::array<char, 128 * 1024> buffer{};
        for (;;) {
            const auto count = ::read(input.get(), buffer.data(), buffer.size());
            if (count < 0) {
                if (errno == EINTR) continue;
                ioError("read snapshot");
            }
            if (count == 0) break;
            ssize_t written = 0;
            while (written < count) {
                const auto n = ::write(output.get(), buffer.data() + written, count - written);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) ioError("write snapshot");
                written += n;
            }
        }
        syncFd(output.get(), "fsync snapshot");
        if (hook) hook(SnapshotStage::BeforeRename);
        if (::rename(temporary.c_str(), destination.c_str()) < 0) ioError("replace snapshot");
        renamed = true;
        if (hook) hook(SnapshotStage::AfterRename);
        FileDescriptor parent(::open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (parent.get() < 0) ioError("open snapshot directory");
        syncFd(parent.get(), "fsync snapshot directory");
    } catch (...) {
        const auto failure = std::current_exception();
        std::error_code ignored;
        if (!renamed) fs::remove(temporary, ignored);
        if (renamed)
            throw std::runtime_error("Ontology snapshot replaced, but directory durability is uncertain; retry dumpToFile.");
        std::rethrow_exception(failure);
    }
}
} // namespace smart_home::ontology::detail
