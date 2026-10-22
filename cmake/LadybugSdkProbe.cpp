#include <lbug.hpp>
#include <iostream>
int main() {
    lbug::main::SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 2;
    config.autoCheckpoint = false;
    config.forceCheckpointOnClose = true;
    config.throwOnWalReplayFailure = true;
    lbug::main::Database database(":memory:", config);
    lbug::main::Connection connection(&database);
    auto statement = connection.prepare("RETURN $value");
    if (!statement->isSuccess()) return 1;
    auto result = connection.execute(statement.get(), std::make_pair(std::string("value"), int64_t{42}));
    if (!result->isSuccess() || !result->hasNext() ||
        result->getNext()->getValue(0)->getValue<int64_t>() != 42) return 2;
    std::cout << lbug::main::Version::getVersion();
}
