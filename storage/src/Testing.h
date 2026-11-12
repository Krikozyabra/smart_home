#pragma once
// Private, thread-local fault hooks for deterministic publication tests.
#include <functional>

namespace smart_home::storage_detail {
enum class DumpStage { Backup, Close, FileSync, Rename, DirectorySync };
using DumpHook = std::function<void(DumpStage)>;
void setDumpHook(DumpHook hook);
} // namespace smart_home::storage_detail
