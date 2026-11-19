#include "PersistenceTestSupport.h"
#include "persistence/GenerationManager.h"
#include "persistence/StateLock.h"

#include <iostream>
#include <utility>

using namespace persistence_test;

namespace {

void runWorker() {
    Directory disk;
    Directory ram("/dev/shm");
    StateLock lock(disk.path / "state");
    expectException<std::invalid_argument>(
        [&] { SaveWorker invalid(lock, 0); });
    SaveWorker worker(lock, 1);
    expectException<std::runtime_error>(
        [&] { Snapshot invalid(ram.path, -1); });
    try {
        auto reservation = worker.reserve();
        require(static_cast<bool>(reservation), "Reserve the only slot");
        auto full = worker.reserve();
        require(!full, "Reservations count against capacity");
        auto snapshot = makeSnapshot(ram.path, 2);
        expectException<std::logic_error>(
            [&] { full.submit(std::move(snapshot)); });
        throw std::invalid_argument("Simulate preparation failure");
    } catch (const std::invalid_argument &) {
    }
    require(fs::is_empty(ram.path),
            "Abandoned preparation cleans the snapshot");

    auto original = worker.reserve();
    auto reservation = std::move(original);
    require(!original && reservation, "Move transfers the reserved slot");
    auto snapshot = makeSnapshot(ram.path, 2);
    const auto snapshot_dir = snapshot.directory();
    reservation.submit(std::move(snapshot));
    require(!reservation && snapshot.directory().empty(),
            "Submit transfers snapshot ownership");
    expectException<std::logic_error>(
        [&] { reservation.submit(std::move(snapshot)); });
    require(ready(worker), "Commit wakes poll");
    require(!worker.reserve(),
            "Completed results keep their slots until collected");
    const auto results = worker.collectResults();
    require(results.size() == 1 && results.front().revision == 2 &&
                results.front().success,
            "Collect a successful commit");
    require(!ready(worker, 0) && !fs::exists(snapshot_dir),
            "Collection drains the event pipe and releases the snapshot");

    const GenerationManager manager(lock);
    const auto active = manager.resolveCommitted();
    require(active && active->revision == 2, "Initial generation is active");
    const auto stale = active->path.parent_path() / ".pending-123AbC";
    const auto unrelated = active->path.parent_path() / "notes";
    fs::create_directory(stale);
    fs::create_directory(unrelated);

    worker.reserve().submit(makeSnapshot(ram.path, 1));
    require(ready(worker), "Rejected stale submission also wakes poll");
    const auto rejected = worker.collectResults();
    require(rejected.size() == 1 && !rejected.front().success &&
                manager.resolveCommitted()->name == active->name,
            "A stale snapshot cannot replace CURRENT");

    auto mixed = makeSnapshot(ram.path, 3);
    DeviceStateStore(mixed.directory() / "state.sqlite").storeProperties({}, 4);
    worker.reserve().submit(std::move(mixed));
    require(ready(worker), "Revision mismatch also wakes poll");
    const auto mismatch = worker.collectResults();
    require(mismatch.size() == 1 && !mismatch.front().success &&
                manager.resolveCommitted()->name == active->name,
            "A mismatched SQLite revision cannot replace CURRENT");

    worker.reserve().submit(makeSnapshot(ram.path, 3));
    worker.stop();
    worker.stop();
    require(!worker.reserve(), "A stopped worker rejects reservations");
    const auto drained = worker.collectResults();
    require(drained.size() == 1 && drained.front().success &&
                manager.resolveCommitted()->revision == 3,
            "Stop drains the queued commit");
    require(
        !fs::exists(stale) && !fs::exists(active->path) &&
            fs::exists(unrelated),
        "Rotation cleans only obsolete generations and staging directories");
}

} // namespace

int main() {
    try {
        runWorker();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
