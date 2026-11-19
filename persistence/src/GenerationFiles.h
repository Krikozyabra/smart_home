#pragma once

#include "persistence/PersistenceTypes.h"

#include <optional>
#include <string_view>

namespace smart_home::persistence::detail {

// A malformed name is never eligible for startup or rotation.
std::optional<Revision> generationRevision(std::string_view name);
std::optional<std::string> readCurrent(const std::filesystem::path &state);
std::string fileSha256(const std::filesystem::path &path, int destination = -1);
std::string formatManifest(Revision revision, const std::string &sqlite_hash,
                           const std::string &ontology_hash);
void validateSQLiteRevision(const std::filesystem::path &path,
                            Revision revision);

} // namespace smart_home::persistence::detail
