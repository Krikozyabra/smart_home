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
};
} // namespace smart_home::ontology::detail
