#include "logging/Logging.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <ostream>
#include <stdexcept>

namespace smart_home::logging {
namespace {
constexpr std::array<const char*, 4> flags{
    "--log-level", "--log-file", "--log-max-size-mib", "--log-max-files"};
constexpr std::array<const char*, 4> variables{
    "SMART_HOME_LOG_LEVEL", "SMART_HOME_LOG_FILE", "SMART_HOME_LOG_MAX_SIZE_MIB", "SMART_HOME_LOG_MAX_FILES"};
std::size_t positive(std::string_view value) {
    std::size_t number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || number == 0)
        throw std::invalid_argument("expected a positive integer");
    return number;
}
void apply(Config& config, std::size_t field, std::string_view value) {
    switch (field) {
    case 0: {
        std::string lower(value);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        constexpr std::array<const char*, 7> levels{"trace", "debug", "info", "warn", "error", "critical", "off"};
        const auto found = std::find(levels.begin(), levels.end(), lower);
        if (found == levels.end()) throw std::invalid_argument("expected trace/debug/info/warn/error/critical/off");
        config.level = static_cast<Level>(found - levels.begin());
        break;
    }
    case 1: config.file_path = value; break;
    case 2: {
        const auto mib = positive(value);
        constexpr std::size_t unit = 1024 * 1024;
        if (mib > std::numeric_limits<std::size_t>::max() / unit)
            throw std::invalid_argument("file size is too large");
        config.max_file_size = mib * unit;
        break;
    }
    case 3: {
        const auto count = positive(value);
        if (count > 200000) throw std::invalid_argument("archive count must be 1..200000");
        config.max_rotated_files = count;
        break;
    }
    }
}
} // namespace

Config resolveConfig(int argc, const char* const* argv, std::ostream& warnings) {
    // Collect CLI first so overridden environment values are never parsed.
    std::array<std::string_view, 4> values{};
    std::array<bool, 4> supplied{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        const auto equal = argument.find('=');
        const auto name = argument.substr(0, equal);
        const auto found = std::find(flags.begin(), flags.end(), name);
        if (found == flags.end()) throw std::invalid_argument("Unknown argument: " + std::string(name));
        const auto field = static_cast<std::size_t>(found - flags.begin());
        if (equal != std::string_view::npos) values[field] = argument.substr(equal + 1);
        else {
            if (++i == argc || std::string_view(argv[i]).substr(0, 2) == "--")
                throw std::invalid_argument("Missing value for " + std::string(name));
            values[field] = argv[i];
        }
        supplied[field] = true; // Last occurrence wins.
    }
    Config config;
    for (std::size_t field = 0; field < flags.size(); ++field) {
        if (supplied[field]) {
            try { apply(config, field, values[field]); }
            catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::string(flags[field]) + ": " + error.what());
            }
        } else if (const auto* value = std::getenv(variables[field])) {
            try { apply(config, field, value); }
            catch (const std::invalid_argument& error) {
                warnings << "Invalid " << variables[field] << ": " << error.what() << "; using default.\n";
            }
        }
    }
    return config;
}
} // namespace smart_home::logging
