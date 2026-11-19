#include "TestUtil.h"
#include "logging/Logging.h"
#include "logging/src/Testing.h"
#include "storage/InMemoryDeviceStorage.h"
#include "storage/SQLiteDeviceStorage.h"
#include "storage/src/Testing.h"

#include <sqlite3.h>
#include <spdlog/sinks/ostream_sink.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using smart_home::SQLiteDeviceStorage;
using smart_home::storage_detail::DumpStage;
using smart_home::storage_detail::setDumpHook;
namespace logging = smart_home::logging;

namespace {
struct Directory {
    fs::path path;
    Directory() {
        auto pattern =
            (fs::temp_directory_path() / "smart-home-sqlite-dump-XXXXXX")
                .string();
        if (!::mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create test directory");
        path = pattern;
    }
    ~Directory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

struct CurrentDirectory {
    fs::path saved = fs::current_path();
    ~CurrentDirectory() {
        std::error_code ignored;
        fs::current_path(saved, ignored);
    }
};

struct Hook {
    explicit Hook(smart_home::storage_detail::DumpHook hook) {
        setDumpHook(std::move(hook));
    }
    ~Hook() { setDumpHook({}); }
};

struct RawDatabase {
    std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> db{nullptr,
                                                             sqlite3_close_v2};
    explicit RawDatabase(const fs::path &path) {
        sqlite3 *raw = nullptr;
        const int result = sqlite3_open(path.c_str(), &raw);
        db.reset(raw);
        require(result == SQLITE_OK, "Open test database");
    }
    void exec(const char *sql) {
        require(sqlite3_exec(db.get(), sql, nullptr, nullptr, nullptr) ==
                    SQLITE_OK,
                "Execute test SQL: " + std::string(sqlite3_errmsg(db.get())));
    }
    std::string scalar(const char *sql) {
        sqlite3_stmt *raw = nullptr;
        const int result = sqlite3_prepare_v2(db.get(), sql, -1, &raw, nullptr);
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(
            raw, sqlite3_finalize);
        require(result == SQLITE_OK && sqlite3_step(raw) == SQLITE_ROW,
                "Read test scalar");
        return reinterpret_cast<const char *>(sqlite3_column_text(raw, 0));
    }
};

std::string bytes(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.is_open(), "Read snapshot bytes");
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
}

void noTemporaryFiles(const Directory &directory) {
    for (const auto &entry : fs::directory_iterator(directory.path))
        require(entry.path().filename().string().find(".sqlite-dump-") != 0,
                "Handled failure cleans temporary database and sidecars");
}

void requireSnapshot(const fs::path &path, std::size_t count) {
    RawDatabase raw(path);
    require(raw.scalar("PRAGMA integrity_check") == "ok",
            "Snapshot SQLite integrity");
    require(raw.scalar("PRAGMA journal_mode") == "delete",
            "Snapshot is independent of WAL");
    SQLiteDeviceStorage restored(path.string());
    require(restored.getAll().size() == count, "Snapshot inventory size");
}

void memorySnapshots() {
    Directory directory;
    const auto target = directory.path / "dump.db";
    SQLiteDeviceStorage source(":memory:");
    smart_home::IDeviceStorage &storage = source;
    storage.dumpToFile(target.string());
    requireSnapshot(target, 0);
    const auto device = source.insert("physical", "driver", "Лампа's");
    requireSnapshot(target, 0);
    storage.dumpToFile(target.string());
    {
        SQLiteDeviceStorage restored(target.string());
        require(restored.findByPhysicalId("physical", "driver") == device,
                "Snapshot preserves IDs and UTF-8 values");
    }
    const auto old_bytes = bytes(target);
    source.insert("another", "driver", "");
    require(bytes(target) == old_bytes,
            "Source mutations do not update the snapshot");
    storage.dumpToFile(target.string());
    requireSnapshot(target, 2);
    noTemporaryFiles(directory);
}

void unsupportedMemoryDump() {
    Directory directory;
    const auto existing = directory.path / "existing.db";
    const auto missing = directory.path / "missing.db";
    {
        std::ofstream file(existing);
        file << "existing content";
    }
    const auto baseline = bytes(existing);
    smart_home::InMemoryDeviceStorage source;
    smart_home::IDeviceStorage &storage = source;
    const auto saved = storage.insert("physical", "driver", "Name");
    for (const auto &path : {existing.string(), missing.string(), std::string{}})
        expectException<std::logic_error>([&] { storage.dumpToFile(path); });
    require(bytes(existing) == baseline && !fs::exists(missing),
            "Unsupported dump neither changes nor creates a file");
    require(storage.getAll() == std::vector<smart_home::DeviceRecord>{saved},
            "Unsupported dump preserves inventory");
    storage.close();
    storage.close();
    for (const auto &path : {existing.string(), missing.string(), std::string{}})
        expectException<std::logic_error>([&] { storage.dumpToFile(path); });
    require(bytes(existing) == baseline && !fs::exists(missing),
            "Closed memory storage neither changes nor creates a file");
    noTemporaryFiles(directory);
}

void fullDatabaseAndWal() {
    Directory directory;
    const auto source_path = directory.path / "source.db";
    const auto target = directory.path / "dump.db";
    smart_home::DeviceRecord saved;
    {
        SQLiteDeviceStorage source(source_path.string());
        RawDatabase raw(source_path);
        raw.exec("PRAGMA journal_mode=WAL; PRAGMA wal_autocheckpoint=0;");
        saved = source.insert("physical", "driver", "Name");
        raw.exec(
            "CREATE TABLE runtime_values(id INTEGER PRIMARY KEY, value BLOB);"
            "CREATE INDEX value_index ON runtime_values(value);"
            "CREATE VIEW runtime_view AS SELECT hex(value) AS value FROM "
            "runtime_values;"
            "CREATE TRIGGER runtime_update AFTER INSERT ON runtime_values "
            "BEGIN UPDATE devices SET name='updated' WHERE local_id=1; END;"
            "INSERT INTO runtime_values VALUES(42, x'00FF1234');"
            "INSERT INTO devices VALUES(500, 'deleted', 'driver', 'High ID');"
            "DELETE FROM devices WHERE local_id=500;"
            "PRAGMA user_version=27;");
        saved.name = "updated";
        require(fs::file_size(source_path.string() + "-wal") > 0,
                "Fixture has actual WAL data");
        source.dumpToFile(target.string());
        require(source.findByPhysicalId("physical", "driver") == saved,
                "Dump leaves working source unchanged");
        require(!fs::exists(target.string() + "-wal") &&
                    !fs::exists(target.string() + "-shm") &&
                    !fs::exists(target.string() + "-journal"),
                "Published database needs no sidecars");
    }
    fs::remove(source_path);
    requireSnapshot(target, 1);
    {
        RawDatabase raw(target);
        require(raw.scalar("SELECT value FROM runtime_view") == "00FF1234",
                "Extra table, BLOB and view survive");
        require(raw.scalar("SELECT count(*) FROM sqlite_schema WHERE name IN "
                           "('value_index', 'runtime_update')") == "2",
                "Index and trigger survive");
        require(raw.scalar("PRAGMA user_version") == "27",
                "Database metadata survives");
        require(raw.scalar(
                    "SELECT seq FROM sqlite_sequence WHERE name='devices'") ==
                    "500",
                "Dump preserves AUTOINCREMENT beyond the largest live ID");
        SQLiteDeviceStorage restored(target.string());
        require(restored.findByPhysicalId("physical", "driver") == saved,
                "File dump preserves records");
        require(restored.insert("new", "driver", "New").local_id == 501,
                "AUTOINCREMENT continues");
        raw.exec("INSERT INTO runtime_values VALUES(43, x'AA');");
        require(restored.findByPhysicalId("physical", "driver")->name ==
                    "updated",
                "Restored trigger can execute");
    }
    noTemporaryFiles(directory);
}

void invalidDestinations() {
    Directory directory;
    const auto source_path = directory.path / "source.db";
    const auto hard_link = directory.path / "hard.db";
    const auto symlink = directory.path / "symbolic.db";
    const auto dangling = directory.path / "dangling.db";
    const auto fifo = directory.path / "fifo";
    const auto parent_alias = directory.path / "parent";
    SQLiteDeviceStorage source(source_path.string());
    const auto saved = source.insert("physical", "driver", "Name");
    fs::create_hard_link(source_path, hard_link);
    fs::create_symlink(source_path, symlink);
    fs::create_symlink(directory.path / "missing.db", dangling);
    fs::create_directory_symlink(directory.path, parent_alias);
    require(::mkfifo(fifo.c_str(), 0600) == 0, "Create FIFO destination");
    const auto baseline = bytes(source_path);
    for (const auto &path :
         {std::string{}, std::string("bad\0path", 8), source_path.string(),
          hard_link.string(), symlink.string(), dangling.string(),
          directory.path.string(), fifo.string(),
          (parent_alias / "source.db").string()}) {
        expectException<std::invalid_argument>(
            [&] { source.dumpToFile(path); });
        if (!path.empty()) {
            expectException<std::invalid_argument>([&] {
                SQLiteDeviceStorage invalid(source_path.string(), path);
            });
        }
    }
    require(bytes(source_path) == baseline,
            "Invalid paths do not modify the working database");
    require(source.findByPhysicalId("physical", "driver") == saved,
            "Working database remains usable");
    const auto missing = directory.path / "missing" / "dump.db";
    expectException<std::runtime_error>(
        [&] { source.dumpToFile(missing.string()); });
    require(!fs::exists(missing.parent_path()),
            "Dump does not create parent directories");
    noTemporaryFiles(directory);
}

void handledFailures() {
    Directory directory;
    const auto target = directory.path / "dump.db";
    SQLiteDeviceStorage source(":memory:");
    source.insert("old", "driver", "Old");
    source.dumpToFile(target.string());
    const auto baseline = bytes(target);
    source.insert("new", "driver", "New");
    for (bool existing : {false, true}) {
        const auto path = existing ? target : directory.path / "first.db";
        for (auto stage : {DumpStage::Backup, DumpStage::Close,
                           DumpStage::FileSync, DumpStage::Rename}) {
            {
                Hook hook([stage](auto point) {
                    if (point == stage)
                        throw std::runtime_error("injected I/O failure");
                });
                try {
                    source.dumpToFile(path.string());
                    require(false, "Injected dump failure must propagate");
                } catch (const std::runtime_error &error) {
                    require(std::string(error.what()).find("SQLite dump ") ==
                                    0 &&
                                std::string(error.what())
                                        .find("injected I/O failure") !=
                                    std::string::npos,
                            "Dump error includes the failed stage and original "
                            "error");
                }
            }
            require(
                existing ? bytes(path) == baseline : !fs::exists(path),
                "Failure before rename leaves previous destination unchanged");
            require(source.getAll().size() == 2,
                    "Failure preserves working database");
            noTemporaryFiles(directory);
        }
    }
    {
        Hook hook([](auto point) {
            if (point == DumpStage::DirectorySync)
                throw std::runtime_error("directory sync failure");
        });
        try {
            source.dumpToFile(target.string());
            require(false, "Directory fsync failure must propagate");
        } catch (const std::runtime_error &error) {
            require(std::string(error.what()).find("durability is uncertain") !=
                        std::string::npos,
                    "Failure after rename reports uncertain durability");
        }
    }
    requireSnapshot(target, 2);
    source.insert("retry", "driver", "Retry");
    source.dumpToFile(target.string());
    requireSnapshot(target, 3);
    noTemporaryFiles(directory);

    // A real SQLite backup error, rather than an injected exception.
    const auto file = directory.path / "source.db";
    SQLiteDeviceStorage file_source(file.string());
    file_source.insert("locked", "driver", "Name");
    RawDatabase lock(file);
    lock.exec("BEGIN EXCLUSIVE;");
    const auto before = bytes(target);
    expectException<std::runtime_error>(
        [&] { file_source.dumpToFile(target.string()); });
    require(bytes(target) == before,
            "Actual backup failure preserves previous snapshot");
    noTemporaryFiles(directory);
    lock.exec("ROLLBACK;");
    file_source.dumpToFile(target.string());
    requireSnapshot(target, 1);
}

void autoDump() {
    Directory directory;
    const auto target = directory.path / "final.db";
    const auto other = directory.path / "other.db";
    {
        SQLiteDeviceStorage source(":memory:", target.string());
        require(!fs::exists(target),
                "Constructor does not open destination resources");
        source.insert("first", "driver", "First");
        source.dumpToFile(target.string());
        source.insert("second", "driver", "Second");
        source.dumpToFile(other.string());
        source.insert("last", "driver", "Last");
    }
    requireSnapshot(target, 3);
    requireSnapshot(other, 2);

    {
        CurrentDirectory cwd;
        fs::current_path(directory.path);
        fs::create_directory("sub");
        {
            SQLiteDeviceStorage source(":memory:", "relative.db");
            fs::current_path(directory.path / "sub");
            source.insert("relative", "driver", "Relative");
            source.dumpToFile("explicit.db");
            source.insert("final", "driver", "Final");
        }
        requireSnapshot(directory.path / "relative.db", 2);
        requireSnapshot(directory.path / "sub" / "explicit.db", 1);
        require(!fs::exists(directory.path / "sub" / "relative.db"),
                "Auto-dump path is fixed at construction");
    }
    int attempts = 0;
    {
        Hook hook([&](auto point) {
            if (point == DumpStage::Backup)
                ++attempts;
        });
        {
            SQLiteDeviceStorage source(":memory:");
            source.insert("no-dump", "driver", "Name");
        }
    }
    require(attempts == 0, "Default constructor performs no auto-dump");
    noTemporaryFiles(directory);
}

std::size_t descriptorsFor(const fs::path &path) {
    std::size_t count = 0;
    for (const auto &entry : fs::directory_iterator("/proc/self/fd")) {
        std::error_code ignored;
        if (fs::read_symlink(entry.path(), ignored) == path)
            ++count;
    }
    return count;
}

void explicitClose() {
    Directory directory;
    const auto source_path = directory.path / "source.db";
    const auto target = directory.path / "final.db";
    const auto other = directory.path / "other.db";
    int attempts = 0;
    Hook hook([&](auto point) {
        if (point == DumpStage::Backup)
            ++attempts;
    });
    {
        SQLiteDeviceStorage source(source_path.string(), target.string());
        smart_home::IDeviceStorage &storage = source;
        const auto saved = storage.insert("first", "driver", "First");
        storage.dumpToFile(other.string());
        storage.insert("last", "driver", "Last");
        attempts = 0;
        storage.close();
        require(attempts == 1, "Explicit close publishes one final snapshot");
        requireSnapshot(target, 2);
        requireSnapshot(other, 1);
        {
            SQLiteDeviceStorage restored(target.string());
            require(restored.findByPhysicalId("first", "driver") == saved,
                    "Explicit close preserves inventory and IDs");
        }
        require(descriptorsFor(source_path) == 0,
                "Explicit close releases the source connection immediately");
        const auto baseline = bytes(target);
        storage.close();
        expectException<std::logic_error>([&] { storage.getAll(); });
        expectException<std::logic_error>(
            [&] { storage.findByPhysicalId("first", "driver"); });
        expectException<std::logic_error>(
            [&] { storage.insert("new", "driver", "Name"); });
        expectException<std::logic_error>(
            [&] { storage.dumpToFile(target.string()); });
        expectException<std::logic_error>(
            [&] { storage.dumpToFile((directory.path / "missing.db").string()); });
        require(bytes(target) == baseline &&
                    !fs::exists(directory.path / "missing.db"),
                "Closed storage leaves snapshot files unchanged");
        // Remove the snapshot to prove neither repeated close nor destruction
        // tries to save it again.
        fs::remove(target);
        storage.close();
    }
    require(attempts == 1 && !fs::exists(target),
            "Repeated close and destructor do not repeat the final dump");
    noTemporaryFiles(directory);
}

void failedClose() {
    Directory directory;
    const auto target = directory.path / "final.db";
    std::ostringstream messages;
    logging::setLevel(logging::Level::Error);
    logging::installTestSink(
        std::make_shared<spdlog::sinks::ostream_sink_mt>(messages));
    for (auto stage : {DumpStage::Backup, DumpStage::Close, DumpStage::FileSync,
                       DumpStage::Rename, DumpStage::DirectorySync}) {
        SQLiteDeviceStorage source(":memory:", target.string());
        smart_home::IDeviceStorage &storage = source;
        const auto first = storage.insert("first", "driver", "First");
        storage.dumpToFile(target.string());
        const auto baseline = bytes(target);
        storage.insert("second", "driver", "Second");
        {
            Hook hook([stage](auto point) {
                if (point == stage)
                    throw std::runtime_error("close injected failure");
            });
            expectException<std::runtime_error>([&] { storage.close(); });
        }
        if (stage != DumpStage::DirectorySync)
            require(bytes(target) == baseline,
                    "Failed close before rename preserves previous snapshot");
        else
            requireSnapshot(target, 2);
        require(storage.findByPhysicalId("first", "driver") == first &&
                    storage.getAll().size() == 2,
                "Failed close retains access to working data");
        storage.insert("retry", "driver", "Retry");
        storage.close();
        storage.close();
        requireSnapshot(target, 3);
        noTemporaryFiles(directory);
    }
    {
        SQLiteDeviceStorage source(":memory:", target.string());
        source.insert("first", "driver", "First");
        fs::remove(target);
        fs::create_symlink(directory.path / "absent.db", target);
        expectException<std::runtime_error>([&] { source.close(); });
        require(fs::is_symlink(target) && source.getAll().size() == 1,
                "Invalidated auto-dump path is a close failure, preserving data");
        fs::remove(target);
        source.close();
        requireSnapshot(target, 1);
    }
    logging::flush();
    logging::shutdown();
    require(messages.str().empty(),
            "Explicit close propagates failures without logging them");
}

void destructorRetriesFailedClose() {
    Directory directory;
    const auto target = directory.path / "final.db";
    {
        SQLiteDeviceStorage source(":memory:", target.string());
        source.insert("first", "driver", "First");
        {
            Hook hook([](auto point) {
                if (point == DumpStage::FileSync)
                    throw std::runtime_error("close injected failure");
            });
            expectException<std::runtime_error>([&] { source.close(); });
        }
        require(!fs::exists(target), "Failed first close publishes no snapshot");
        source.insert("last", "driver", "Last");
    }
    requireSnapshot(target, 2);
    noTemporaryFiles(directory);
}

void failedAutoDump() {
    Directory directory;
    const auto source_path = directory.path / "source.db";
    const auto target = directory.path / "dump.db";
    {
        SQLiteDeviceStorage seed(":memory:");
        seed.dumpToFile(target.string());
    }
    const auto baseline = bytes(target);
    std::ostringstream messages;
    logging::setLevel(logging::Level::Error);
    logging::installTestSink(
        std::make_shared<spdlog::sinks::ostream_sink_mt>(messages));
    {
        Hook hook([](auto point) {
            if (point == DumpStage::FileSync)
                throw std::runtime_error("auto-dump injected failure");
        });
        {
            SQLiteDeviceStorage source(source_path.string(), target.string());
            source.insert("unsaved", "driver", "Name");
        }
    }
    logging::flush();
    logging::shutdown();
    const auto output = messages.str();
    require(output.find("auto-dump injected failure") != std::string::npos &&
                std::count(output.begin(), output.end(), '\n') == 1,
            "Destructor logs a single failure without throwing");
    require(bytes(target) == baseline, "Failed auto-dump retains old snapshot");
    require(descriptorsFor(source_path) == 0,
            "Destructor releases source connection despite dump failure");
    requireSnapshot(source_path, 1);
    noTemporaryFiles(directory);
}

void interruptedPublication() {
    for (bool existing : {false, true}) {
        for (auto stage :
             {DumpStage::Backup, DumpStage::Close, DumpStage::FileSync,
              DumpStage::Rename, DumpStage::DirectorySync}) {
            Directory directory;
            const auto source_path = directory.path / "source.db";
            const auto target = directory.path / "dump.db";
            std::string baseline;
            {
                SQLiteDeviceStorage source(source_path.string());
                source.insert("old", "driver", "Old");
                if (existing) {
                    source.dumpToFile(target.string());
                    baseline = bytes(target);
                }
                source.insert("new", "driver", "New");
            }
            const pid_t child = ::fork();
            require(child >= 0, "Fork dump process");
            if (child == 0) {
                try {
                    SQLiteDeviceStorage source(source_path.string());
                    setDumpHook([stage](auto point) {
                        if (point == stage)
                            ::_exit(73);
                    });
                    source.dumpToFile(target.string());
                } catch (...) {
                }
                ::_exit(74);
            }
            int status = 0;
            pid_t result;
            do {
                result = ::waitpid(child, &status, 0);
            } while (result == -1 && errno == EINTR);
            require(result == child && WIFEXITED(status) &&
                        WEXITSTATUS(status) == 73,
                    "Child stops at the intended publication stage without "
                    "running destructors");
            if (stage == DumpStage::DirectorySync)
                requireSnapshot(target, 2);
            else
                require(
                    existing ? bytes(target) == baseline : !fs::exists(target),
                    "Process interruption cannot publish a partial snapshot");
        }
    }
}
} // namespace

int main() {
    try {
        memorySnapshots();
        unsupportedMemoryDump();
        fullDatabaseAndWal();
        invalidDestinations();
        handledFailures();
        autoDump();
        explicitClose();
        failedClose();
        destructorRetriesFailedClose();
        failedAutoDump();
        interruptedPublication();
        std::cout << "SQLite dump tests passed\n";
    } catch (const std::exception &error) {
        logging::shutdown();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
