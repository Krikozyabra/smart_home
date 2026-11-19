#pragma once

#include "TestUtil.h"
#include "persistence/DeviceStateStore.h"
#include "persistence/SaveWorker.h"
#include "storage/SQLiteDeviceStorage.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <poll.h>
#include <string>

namespace persistence_test {
namespace fs = std::filesystem;
using namespace smart_home::persistence;

class Directory {
  public:
    explicit Directory(const fs::path &base = fs::temp_directory_path()) {
        auto pattern = (base / "smart-home-persistence-XXXXXX").string();
        require(::mkdtemp(pattern.data()) != nullptr,
                "Create temporary test directory");
        path = pattern;
    }
    ~Directory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    Directory(const Directory &) = delete;
    Directory &operator=(const Directory &) = delete;
    fs::path path;
};

inline void writeFile(const fs::path &path, const std::string &text) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << text;
    file.close();
}

inline std::string readFile(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    require(file.is_open(), "Open test file for reading");
    return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
}

inline Snapshot makeSnapshot(const fs::path &ram, Revision revision) {
    Snapshot snapshot(ram, revision);
    const auto path = snapshot.directory() / "state.sqlite";
    smart_home::SQLiteDeviceStorage inventory(path.string());
    DeviceStateStore store(path);
    store.initializeSchema();
    store.storeProperties({}, revision);
    writeFile(snapshot.directory() / "ontology.lbdb", "graph snapshot");
    return snapshot;
}

inline bool ready(const SaveWorker &worker, int timeout = 5000) {
    pollfd fd{worker.eventFd(), POLLIN, 0};
    int result;
    do {
        result = ::poll(&fd, 1, timeout);
    } while (result < 0 && errno == EINTR);
    require(result >= 0, "Poll the worker event pipe");
    return result == 1 && (fd.revents & POLLIN);
}
} // namespace persistence_test
