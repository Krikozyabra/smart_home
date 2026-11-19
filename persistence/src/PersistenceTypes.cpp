#include "persistence/PersistenceTypes.h"
#include "FileUtils.h"
#include "Testing.h"
#include "persistence/PersistenceConfig.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace smart_home::persistence {

Revision nextRevision(Revision revision) {
    if (revision < 0)
        throw std::runtime_error("Runtime revision must be nonnegative.");
    if (revision == std::numeric_limits<Revision>::max())
        throw std::runtime_error("Runtime revision overflow.");
    return revision + 1;
}

Snapshot::Snapshot(const std::filesystem::path &ram_base, Revision revision)
    : revision_(revision) {
    if (revision < 0)
        throw std::runtime_error("Runtime revision must be nonnegative.");
    PERSISTENCE_TEST_POINT("prepare", revision);
    const auto base = detail::absolutePath(ram_base);
    requireRamDirectory(base);
    directory_ = detail::makeTemporaryDirectory(base, "snapshot-");
}

Snapshot::Snapshot(Snapshot &&other) noexcept
    : directory_(std::move(other.directory_)), revision_(other.revision_) {
    other.directory_.clear();
}

Snapshot::~Snapshot() noexcept {
    if (!directory_.empty())
        detail::removeTree(directory_);
}

} // namespace smart_home::persistence
