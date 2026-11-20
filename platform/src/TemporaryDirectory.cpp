#include "platform/TemporaryDirectory.h"
#include "platform/SystemError.h"

#include <unistd.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace smart_home::platform {
namespace fs = std::filesystem;
namespace {

fs::path createDirectory(const fs::path &base, std::string_view prefix) {
    if (base.empty() || base.native().find('\0') != std::string::npos ||
        prefix.find('/') != std::string_view::npos ||
        prefix.find('\0') != std::string_view::npos)
        throw std::invalid_argument(
            "Invalid temporary directory base or prefix.");

    auto pattern =
        (fs::absolute(base) / (std::string(prefix) + "XXXXXX")).string();
    while (!::mkdtemp(pattern.data())) {
        if (errno != EINTR)
            throwSystemError("mkdtemp");
        pattern.replace(pattern.size() - 6, 6, "XXXXXX");
    }
    try {
        return fs::path(pattern);
    } catch (...) {
        retryInterrupted([&] { return ::rmdir(pattern.c_str()); });
        throw;
    }
}

} // namespace

TemporaryDirectory::TemporaryDirectory(const std::filesystem::path &base,
                                       std::string_view prefix)
    : path_(createDirectory(base, prefix)) {}

TemporaryDirectory::~TemporaryDirectory() noexcept { cleanup(); }

TemporaryDirectory::TemporaryDirectory(TemporaryDirectory &&other) noexcept
    : preserve_(other.preserve_) {
    path_.swap(other.path_);
}

TemporaryDirectory &
TemporaryDirectory::operator=(TemporaryDirectory &&other) noexcept {
    if (this != &other) {
        cleanup();
        path_.clear();
        path_.swap(other.path_);
        preserve_ = other.preserve_;
    }
    return *this;
}

void TemporaryDirectory::cleanup() noexcept {
    if (path_.empty() || preserve_)
        return;
    try {
        std::error_code error;
        fs::remove_all(path_, error);
    } catch (...) {
        // Очистка в деструкторе не должна прерывать раскрутку стека.
    }
}

} // namespace smart_home::platform
