#pragma once
// Private test seams; never included by a public consumer.
#include "ontology/LadybugOntologyStorage.h"
#include <filesystem>
#include <functional>

namespace smart_home::ontology::detail {
enum class SnapshotStage { BeforeCopy, BeforeRename, AfterRename, BeforeReopen };
using SnapshotHook = std::function<void(SnapshotStage)>;
struct OntologyTestAccess {
    static void setSnapshotHook(LadybugOntologyStorage&, SnapshotHook);
    static std::filesystem::path runtimePath(const LadybugOntologyStorage&);
    // Wake the actual worker through the same branch as a timer deadline and wait
    // for its completed attempt. No sleeps or public production test controls.
    static void triggerPeriodicSync(LadybugOntologyStorage&);
    static void waitForPeriodicSync(LadybugOntologyStorage&);
};
} // namespace smart_home::ontology::detail
