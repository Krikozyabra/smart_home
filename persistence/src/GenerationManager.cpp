#include "persistence/GenerationManager.h"
#include "FileUtils.h"
#include "GenerationFiles.h"
#include "persistence/StateLock.h"

#include <openssl/evp.h>

#include <array>
#include <charconv>
#include <memory>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace smart_home::persistence {
namespace detail {

std::optional<Revision> generationRevision(std::string_view name) {
    if (name.substr(0, 4) != "gen-" || name.size() > 100)
        return std::nullopt;
    name.remove_prefix(4);
    const auto separator = name.find('-');
    if (separator == std::string_view::npos || separator == 0 ||
        separator + 1 == name.size() ||
        name.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-",
            separator + 1) != std::string_view::npos)
        return std::nullopt;
    Revision revision{};
    const char *end = name.data() + separator;
    const auto parsed = std::from_chars(name.data(), end, revision);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        return std::nullopt;
    return revision;
}

std::optional<std::string> readCurrent(const fs::path &state) {
    const auto pointer = state / "CURRENT";
    if (!fs::exists(fs::symlink_status(pointer)))
        return std::nullopt;
    auto name = readSmallFile(pointer);
    if (name.empty() || name.back() != '\n')
        throw std::runtime_error("Invalid runtime CURRENT.");
    name.pop_back();
    if (!generationRevision(name))
        throw std::runtime_error("Invalid runtime generation name.");
    return name;
}

std::string fileSha256(const fs::path &path, int destination) {
    const FileDescriptor file(openRegularFile(path));
    const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("SHA-256 initialization failed.");
    std::array<char, 65536> buffer;
    while (const auto count =
               readBytes(file.get(), buffer.data(), buffer.size())) {
        const auto size = static_cast<std::size_t>(count);
        if (EVP_DigestUpdate(ctx.get(), buffer.data(), size) != 1)
            throw std::runtime_error("SHA-256 update failed.");
        if (destination != -1)
            writeBytes(destination, {buffer.data(), size});
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> bytes;
    unsigned length = 0;
    if (EVP_DigestFinal_ex(ctx.get(), bytes.data(), &length) != 1 ||
        length != 32)
        throw std::runtime_error("SHA-256 finalization failed.");
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(2 * length);
    for (unsigned i = 0; i < length; ++i) {
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 0xf];
    }
    return result;
}

std::string formatManifest(Revision revision, const std::string &sqlite_hash,
                           const std::string &ontology_hash) {
    return "SMART_HOME_RUNTIME 1\n" + std::to_string(revision) + "\n" +
           sqlite_hash + "\n" + ontology_hash + "\n";
}

} // namespace detail

GenerationManager::GenerationManager(const StateLock &lock) noexcept
    : lock_(lock) {}

std::optional<CommittedGeneration> GenerationManager::resolveCommitted() const {
    try {
        auto name = detail::readCurrent(lock_.directory());
        if (!name)
            return std::nullopt;
        const Revision revision = *detail::generationRevision(*name);
        const auto generations = lock_.directory() / "generations";
        auto path = generations / *name;
        if (!fs::is_directory(fs::symlink_status(generations)) ||
            !fs::is_directory(fs::symlink_status(path)))
            throw std::runtime_error("Missing committed generation.");
        const auto manifest = detail::readSmallFile(path / "metadata");
        const auto sqlite = path / "state.sqlite";
        const auto state_hash = detail::fileSha256(sqlite);
        const auto ontology_hash = detail::fileSha256(path / "ontology.lbdb");
        if (manifest !=
            detail::formatManifest(revision, state_hash, ontology_hash))
            throw std::runtime_error("Invalid generation manifest.");
        detail::validateSQLiteRevision(sqlite, revision);
        return CommittedGeneration{std::move(path), revision, std::move(*name)};
    } catch (const std::runtime_error &) {
        throw std::runtime_error("Corrupt or mixed runtime generation");
    }
}

} // namespace smart_home::persistence
