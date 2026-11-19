#include "SQLiteDump.h"
#include "Testing.h"

#include <sqlite3.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

namespace smart_home::storage_detail {
namespace {
thread_local DumpHook dump_hook;

void checkpoint(DumpStage stage) {
    if (dump_hook)
        dump_hook(stage);
}

[[noreturn]] void systemFailure() {
    throw std::system_error(errno, std::generic_category());
}

class Descriptor {
  public:
    explicit Descriptor(int fd = -1) noexcept : fd_(fd) {}
    ~Descriptor() noexcept {
        if (fd_ != -1)
            ::close(fd_);
    }
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    int get() const noexcept { return fd_; }
    void close() {
        const int fd = std::exchange(fd_, -1);
        // On Linux even EINTR releases the descriptor; retrying could close an
        // unrelated descriptor acquired by another thread.
        if (::close(fd) != 0)
            systemFailure();
    }

  private:
    int fd_;
};

struct TemporaryFile {
    std::string path;
    bool created = false;
    explicit TemporaryFile(const fs::path &parent)
        : path((parent / ".sqlite-dump-XXXXXX").string()) {}
    ~TemporaryFile() noexcept {
        if (!created)
            return;
        ::unlink(path.c_str());
        // Close the temporary SQLite connection before removing any sidecars.
        for (const char *suffix : {"-journal", "-wal", "-shm"}) {
            try {
                ::unlink((path + suffix).c_str());
            } catch (...) {
                // Cleanup must never replace the original publication error.
            }
        }
    }
};

using DatabasePtr = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>;
using BackupPtr =
    std::unique_ptr<sqlite3_backup, decltype(&sqlite3_backup_finish)>;

void checkSQLite(int result, sqlite3 *database) {
    if (result != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
}
} // namespace

void setDumpHook(DumpHook hook) { dump_hook = std::move(hook); }

fs::path validateDumpPath(sqlite3 *source, const std::string &path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::invalid_argument(
            "SQLite dump destination must be a nonempty path without NUL");

    const fs::path destination = fs::absolute(path);
    struct stat target{};
    if (::lstat(destination.c_str(), &target) != 0) {
        if (errno == ENOENT)
            return destination;
        throw std::runtime_error(
            "SQLite dump destination validation failed: " +
            std::error_code(errno, std::generic_category()).message());
    }
    if (!S_ISREG(target.st_mode))
        throw std::invalid_argument(
            "SQLite dump destination must be a regular file, not a symlink");

    const char *source_path = sqlite3_db_filename(source, "main");
    if (source_path && *source_path) {
        struct stat origin{};
        if (::stat(source_path, &origin) != 0)
            throw std::runtime_error(
                "SQLite dump source validation failed: " +
                std::error_code(errno, std::generic_category()).message());
        if (origin.st_dev == target.st_dev && origin.st_ino == target.st_ino)
            throw std::invalid_argument(
                "SQLite dump destination aliases the working database");
    }
    return destination;
}

void dumpToFile(sqlite3 *source, const std::string &path) {
    const fs::path destination = validateDumpPath(source, path);
    const fs::path parent = destination.parent_path();
    const char *stage = "open destination directory";
    try {
        Descriptor directory(
            ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (directory.get() == -1)
            systemFailure();

        stage = "create temporary file";
        TemporaryFile temporary(parent);
        Descriptor file(::mkstemp(temporary.path.data()));
        if (file.get() == -1)
            systemFailure();
        temporary.created = true;

        stage = "open temporary database";
        sqlite3 *raw_database = nullptr;
        const int opened =
            sqlite3_open_v2(temporary.path.c_str(), &raw_database,
                            SQLITE_OPEN_READWRITE, nullptr);
        DatabasePtr database(raw_database, sqlite3_close_v2);
        if (opened != SQLITE_OK)
            throw std::runtime_error(database ? sqlite3_errmsg(database.get())
                                              : sqlite3_errstr(opened));

        stage = "initialize backup";
        BackupPtr backup(
            sqlite3_backup_init(database.get(), "main", source, "main"),
            sqlite3_backup_finish);
        if (!backup)
            throw std::runtime_error(sqlite3_errmsg(database.get()));

        stage = "backup";
        checkpoint(DumpStage::Backup);
        const int copied = sqlite3_backup_step(backup.get(), -1);
        stage = "finish backup";
        const int finished = sqlite3_backup_finish(backup.release());
        if (copied != SQLITE_DONE)
            throw std::runtime_error("backup step: " +
                                     std::string(sqlite3_errstr(copied)));
        checkSQLite(finished, database.get());

        // A WAL source may carry WAL mode in its header. Convert the private
        // result to DELETE mode before closing so the published file is
        // standalone.
        stage = "finalize standalone database";
        checkSQLite(sqlite3_exec(database.get(), "PRAGMA journal_mode=DELETE;",
                                 nullptr, nullptr, nullptr),
                    database.get());

        stage = "close temporary database";
        checkpoint(DumpStage::Close);
        checkSQLite(sqlite3_close(database.get()), database.get());
        database.release();

        stage = "fsync temporary file";
        checkpoint(DumpStage::FileSync);
        if (::fsync(file.get()) != 0)
            systemFailure();
        stage = "close temporary file";
        file.close();

        stage = "rename";
        checkpoint(DumpStage::Rename);
        if (::rename(temporary.path.c_str(), destination.c_str()) != 0)
            systemFailure();

        stage = "fsync destination directory (durability is uncertain after "
                "rename)";
        checkpoint(DumpStage::DirectorySync);
        if (::fsync(directory.get()) != 0)
            systemFailure();
    } catch (const std::exception &error) {
        throw std::runtime_error("SQLite dump " + std::string(stage) +
                                 " failed: " + error.what());
    }
}
} // namespace smart_home::storage_detail
