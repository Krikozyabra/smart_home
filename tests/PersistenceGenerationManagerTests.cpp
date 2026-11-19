#include "PersistenceTestSupport.h"
#include "persistence/GenerationManager.h"
#include "persistence/StateLock.h"

#include <iostream>

using namespace persistence_test;

namespace {

void validateGenerations() {
    Directory disk;
    Directory ram("/dev/shm");
    StateLock lock(disk.path / "state%?#");
    const GenerationManager manager(lock);
    const auto pointer = lock.directory() / "CURRENT";
    const auto orphan = lock.directory() / "generations" / "gen-8-orphan";
    fs::create_directory(orphan);
    require(!manager.resolveCommitted(),
            "Build a clean state only without CURRENT");
    expectException<std::runtime_error>(
        [&] { StateLock duplicate(lock.directory()); });

    {
        SaveWorker worker(lock, 1);
        auto reservation = worker.reserve();
        reservation.submit(makeSnapshot(ram.path, 7));
        worker.stop();
        const auto results = worker.collectResults();
        require(results.size() == 1 && results[0].success,
                "Publish fixture generation");
    }
    const auto committed = manager.resolveCommitted();
    require(committed && committed->revision == 7 &&
                committed->path ==
                    lock.directory() / "generations" / committed->name,
            "Resolve canonical generation and revision");

    for (const std::string malformed :
         {"", "../escape\n", "gen-x-suffix\n", "gen-7-\n", "gen--7-suffix\n",
          "gen-9223372036854775808-suffix\n"}) {
        writeFile(pointer, malformed);
        expectException<std::runtime_error>(
            [&] { manager.resolveCommitted(); });
    }
    writeFile(pointer, committed->name);
    expectException<std::runtime_error>([&] { manager.resolveCommitted(); });

    const auto incorrect_name = "gen-8-" + committed->name.substr(6);
    fs::rename(committed->path, committed->path.parent_path() / incorrect_name);
    writeFile(pointer, incorrect_name + "\n");
    expectException<std::runtime_error>([&] { manager.resolveCommitted(); });
    fs::rename(committed->path.parent_path() / incorrect_name, committed->path);
    writeFile(pointer, committed->name + "\n");

    const auto graph = committed->path / "ontology.lbdb";
    const auto graph_bytes = readFile(graph);
    writeFile(graph, "different graph");
    expectException<std::runtime_error>([&] { manager.resolveCommitted(); });
    writeFile(graph, graph_bytes);

    const auto sqlite = committed->path / "state.sqlite";
    const auto sqlite_bytes = readFile(sqlite);
    fs::rename(sqlite, disk.path / "other.sqlite");
    fs::create_symlink(disk.path / "other.sqlite", sqlite);
    expectException<std::runtime_error>([&] { manager.resolveCommitted(); });
    fs::remove(sqlite);
    writeFile(sqlite, sqlite_bytes);
    require(manager.resolveCommitted()->name == committed->name,
            "Validation never rewrites CURRENT or repairs saved files");
}

} // namespace

int main() {
    try {
        validateGenerations();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
