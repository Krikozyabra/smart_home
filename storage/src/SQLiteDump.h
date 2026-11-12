#pragma once

#include <filesystem>
#include <string>

struct sqlite3;

namespace smart_home::storage_detail {
// Validation does not open destination resources. The absolute path also fixes
// the meaning of relative auto-dump paths before the caller can change cwd.
std::filesystem::path validateDumpPath(sqlite3 *source,
                                       const std::string &path);
void dumpToFile(sqlite3 *source, const std::string &path);
} // namespace smart_home::storage_detail
