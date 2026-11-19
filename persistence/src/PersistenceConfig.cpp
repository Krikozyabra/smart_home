#include "persistence/PersistenceConfig.h"
#include "FileUtils.h"

#include <linux/magic.h>
#include <stdexcept>
#include <sys/statfs.h>

namespace smart_home::persistence {

void validateConfig(const PersistenceConfig &config) {
    if (config.state_dir.empty() || config.ram_dir.empty() ||
        config.capacity == 0 ||
        config.state_dir.native().find('\0') != std::string::npos ||
        config.ram_dir.native().find('\0') != std::string::npos)
        throw std::invalid_argument(
            "Runtime paths and snapshot capacity must be nonempty/positive.");
}

void requireRamDirectory(const std::filesystem::path &path) {
    const auto directory = detail::absolutePath(path);
    if (!std::filesystem::is_directory(directory))
        throw std::runtime_error("Runtime workspace directory is unavailable.");
    struct statfs info{};
    if (detail::retryInterrupted(
            [&] { return ::statfs(directory.c_str(), &info); }) < 0)
        detail::systemError("statfs RAM directory");
    if (info.f_type != TMPFS_MAGIC && info.f_type != RAMFS_MAGIC)
        throw std::runtime_error("Runtime workspace must be on tmpfs/ramfs; "
                                 "disk fallback is disabled.");
}

} // namespace smart_home::persistence
