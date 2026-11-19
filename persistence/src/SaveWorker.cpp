#include "persistence/SaveWorker.h"
#include "FileUtils.h"
#include "GenerationFiles.h"
#include "Testing.h"
#include "logging/Logging.h"
#include "persistence/StateLock.h"

#include <array>
#include <condition_variable>
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

namespace smart_home::persistence {
namespace {

constexpr char pending_prefix[] = ".pending-";
constexpr auto pending_prefix_size = sizeof(pending_prefix) - 1;

class EventPipe {
  public:
    EventPipe() : EventPipe(openPipe()) {}
    int reader() const noexcept { return reader_.get(); }

    void notify() const noexcept {
        const char byte = 1;
        if (detail::retryInterrupted(
                [&] { return ::write(writer_.get(), &byte, 1); }) < 0 &&
            errno != EAGAIN)
            logging::log(logging::Level::Error, "Persistence",
                         "Save event notification failed.");
    }

    void drain() const {
        char bytes[128];
        for (;;) {
            const auto count = detail::retryInterrupted(
                [&] { return ::read(reader(), bytes, sizeof(bytes)); });
            if (count < 0 && errno == EAGAIN)
                return;
            if (count < 0)
                detail::systemError("read save event pipe");
            if (count == 0)
                throw std::runtime_error(
                    "Save event pipe unexpectedly closed.");
        }
    }

  private:
    explicit EventPipe(std::array<int, 2> fds)
        : reader_(fds[0]), writer_(fds[1]) {}

    static std::array<int, 2> openPipe() {
        std::array<int, 2> fds;
        if (detail::retryInterrupted([&] {
                return ::pipe2(fds.data(), O_NONBLOCK | O_CLOEXEC);
            }) < 0)
            detail::systemError("open save event pipe");
        return fds;
    }

    detail::FileDescriptor reader_;
    detail::FileDescriptor writer_;
};

struct PendingDirectory {
    explicit PendingDirectory(const fs::path &base)
        : path(detail::makeTemporaryDirectory(base, pending_prefix)) {}
    ~PendingDirectory() {
        if (!path.empty())
            detail::removeTree(path);
    }
    void renameTo(fs::path destination) {
        detail::renamePath(path, destination);
        path.swap(destination);
    }
    fs::path path;
};

std::string copyGenerationFile(const fs::path &from, const fs::path &to) {
    detail::FileDescriptor destination(
        detail::openPath(to, O_WRONLY | O_CREAT | O_EXCL));
    auto hash = detail::fileSha256(from, destination.get());
    detail::syncFile(destination.get());
    destination.close();
    return hash;
}

void rotateGenerations(const fs::path &active) {
    for (const auto &entry : fs::directory_iterator(active.parent_path())) {
        if (entry.path() == active || !fs::is_directory(entry.symlink_status()))
            continue;
        const auto name = entry.path().filename().string();
        const bool pending =
            name.size() == pending_prefix_size + 6 &&
            name.compare(0, pending_prefix_size, pending_prefix) == 0 &&
            name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQ"
                                   "RSTUVWXYZ0123456789",
                                   pending_prefix_size) == std::string::npos;
        if (pending || detail::generationRevision(name)) {
            std::error_code error;
            do {
                fs::remove_all(entry.path(), error);
            } while (error == std::errc::interrupted);
            if (error)
                throw std::system_error(error, "remove obsolete generation");
        }
    }
}

void publish(const fs::path &state, const Snapshot &snapshot) {
    const Revision revision = snapshot.revision();
    const auto previous = detail::readCurrent(state);
    if (previous && revision < *detail::generationRevision(*previous))
        throw std::runtime_error("Cannot publish an older runtime revision.");

    const auto &source = snapshot.directory();
    detail::validateSQLiteRevision(source / "state.sqlite", revision);
    const auto generations = state / "generations";
    PendingDirectory temporary(generations);
    PERSISTENCE_TEST_POINT("before-files", revision);
    const auto state_hash = copyGenerationFile(source / "state.sqlite",
                                               temporary.path / "state.sqlite");
    PERSISTENCE_TEST_POINT("state.sqlite", revision);
    const auto ontology_hash = copyGenerationFile(
        source / "ontology.lbdb", temporary.path / "ontology.lbdb");
    PERSISTENCE_TEST_POINT("ontology.lbdb", revision);
    detail::writeSyncedFile(
        temporary.path / "metadata",
        detail::formatManifest(revision, state_hash, ontology_hash));
    PERSISTENCE_TEST_POINT("metadata", revision);
    detail::syncDirectory(temporary.path);

    const auto name =
        "gen-" + std::to_string(revision) + "-" +
        temporary.path.filename().string().substr(pending_prefix_size);
    const auto generation = generations / name;
    temporary.renameTo(generation);
    detail::syncDirectory(generations);
    PERSISTENCE_TEST_POINT("generation", revision);

    const auto current_tmp = state / "CURRENT.tmp";
    detail::unlinkIfPresent(current_tmp);
    try {
        detail::writeSyncedFile(current_tmp, name + "\n");
        PERSISTENCE_TEST_POINT("before-current", revision);
        detail::renamePath(current_tmp, state / "CURRENT");
    } catch (...) {
        detail::removeTree(current_tmp);
        throw;
    }
    temporary.path.clear();
    PERSISTENCE_TEST_POINT("after-current", revision);
    // Both generations must survive until the new CURRENT is durable.
    detail::syncDirectory(state);
    try {
        PERSISTENCE_TEST_POINT("committed", revision);
        rotateGenerations(generation);
    } catch (...) {
        logging::log(logging::Level::Warn, "Persistence",
                     "Generation rotation failed.");
    }
}

} // namespace

class SaveWorker::Impl {
  public:
    Impl(const StateLock &lock, std::size_t capacity)
        : state_(lock.directory()), capacity_(capacity) {
        results_.reserve(capacity_);
        thread_ = std::thread([this] { work(); });
    }

    bool reserve() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || occupied_ == capacity_)
            return false;
        ++occupied_;
        return true;
    }

    void release() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        --occupied_;
    }

    void submit(Snapshot &&snapshot) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_ || snapshot.directory().empty())
                throw std::logic_error(
                    "Cannot submit snapshot to the save worker.");
            queue_.emplace_back(std::move(snapshot));
        }
        changed_.notify_one();
    }

    std::vector<SaveResult> collectResults() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto results = results_;
        events_.drain();
        results_.clear();
        occupied_ -= results.size();
        return results;
    }

    int eventFd() const noexcept { return events_.reader(); }

    void stop() {
        std::call_once(stop_once_, [this] {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopping_ = true;
            }
            changed_.notify_one();
            thread_.join();
        });
    }

  private:
    std::optional<Snapshot> takeSnapshot() {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (queue_.empty())
            return std::nullopt;
        Snapshot snapshot(std::move(queue_.front()));
        queue_.pop_front();
        return snapshot;
    }

    void work() noexcept {
        while (auto snapshot = takeSnapshot()) {
            SaveResult result{snapshot->revision(), false};
            try {
                publish(state_, *snapshot);
                result.success = true;
            } catch (...) {
                // Report the failure to the owner of the live state.
            }
            snapshot.reset();
            std::lock_guard<std::mutex> lock(mutex_);
            results_.push_back(result);
            events_.notify();
        }
    }

    const fs::path state_;
    const std::size_t capacity_;
    std::size_t occupied_ = 0;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<Snapshot> queue_;
    std::vector<SaveResult> results_;
    bool stopping_ = false;
    std::once_flag stop_once_;
    EventPipe events_;
    std::thread thread_;
};

SaveWorker::SaveWorker(const StateLock &lock, std::size_t capacity) {
    if (capacity == 0)
        throw std::invalid_argument("Snapshot capacity must be positive.");
    impl_ = std::make_unique<Impl>(lock, capacity);
}

SaveWorker::~SaveWorker() noexcept { stop(); }

Reservation SaveWorker::reserve() {
    return Reservation(impl_->reserve() ? this : nullptr);
}
void SaveWorker::release() noexcept { impl_->release(); }
void SaveWorker::submit(Snapshot &&snapshot) {
    impl_->submit(std::move(snapshot));
}
std::vector<SaveResult> SaveWorker::collectResults() {
    return impl_->collectResults();
}
int SaveWorker::eventFd() const noexcept { return impl_->eventFd(); }
void SaveWorker::stop() { impl_->stop(); }

} // namespace smart_home::persistence
