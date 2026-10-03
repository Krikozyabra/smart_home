#include "logging/Logging.h"
#ifdef SMART_HOME_LOGGING_TESTING
#include "Testing.h"
#endif

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace smart_home::logging {
namespace {
struct State {
    std::mutex mutex;
    std::shared_ptr<spdlog::logger> logger;
    Config config;
    bool initialized = false;
    bool session_active = false;
    Level threshold = Level::Warn;
};
State& state() { static State value; return value; }

bool valid(Level level) noexcept { return level >= Level::Trace && level <= Level::Off; }
void fallback(std::string_view component, std::string_view message) noexcept {
    // Keep each emergency record intact; never recurse through spdlog.
    try {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        std::fputs("[logging] [", stderr);
        if (!component.empty()) std::fwrite(component.data(), 1, component.size(), stderr);
        std::fputs("] ", stderr);
        if (!message.empty()) std::fwrite(message.data(), 1, message.size(), stderr);
        std::fputc('\n', stderr);
    } catch (...) {
        std::fputs("[logging] Emergency diagnostic output failed.\n", stderr);
    }
}
std::atomic<long long>& errorTimer() noexcept {
    static std::atomic<long long> last{-1};
    return last;
}
void sinkError() noexcept {
    auto& last = errorTimer();
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous = last.load();
    if ((previous == -1 || now - previous >= 5) && last.compare_exchange_strong(previous, now))
        fallback("output", "Log sink failed; diagnostic output may be incomplete.");
}
bool same(const Config& a, const Config& b) {
    return a.level == b.level && a.file_path == b.file_path &&
           a.max_file_size == b.max_file_size && a.max_rotated_files == b.max_rotated_files;
}
std::shared_ptr<spdlog::logger> snapshot() {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.logger;
}
} // namespace

void initialize(const Config& config) {
    if (!valid(config.level)) throw std::invalid_argument("Invalid logging level");
    if (config.max_file_size == 0) throw std::invalid_argument("Log file size must be positive");
    if (config.max_rotated_files == 0 || config.max_rotated_files > 200000)
        throw std::invalid_argument("Archive count must be 1..200000");
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.initialized) {
        if (!same(config, s.config)) throw std::logic_error("Logging already initialized with different configuration");
        return;
    }
    s.config = config;
    s.threshold = config.level;
    try {
        std::vector<spdlog::sink_ptr> sinks;
        auto console = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
        sinks.push_back(console);
        if (!config.file_path.empty()) {
            try {
                auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    config.file_path, config.max_file_size, config.max_rotated_files);
                file->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
                sinks.push_back(file);
            } catch (...) {
                fallback("startup", "Cannot create log file; using stderr only.");
            }
        }
        s.logger = std::make_shared<spdlog::logger>("smart_home", sinks.begin(), sinks.end());
        s.logger->set_level(static_cast<spdlog::level::level_enum>(config.level));
        s.logger->flush_on(spdlog::level::warn);
        s.logger->set_error_handler([](const std::string&) { sinkError(); });
    } catch (...) {
        s.logger.reset();
        fallback("startup", "Cannot create logger; using minimal stderr output.");
    }
    s.initialized = true;
}

bool enabled(Level level) noexcept {
    try {
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        return valid(level) && level != Level::Off && s.threshold != Level::Off && level >= s.threshold;
    } catch (...) { return false; }
}

void log(Level level, std::string_view component, std::string_view message, Context context) noexcept {
    try {
        std::shared_ptr<spdlog::logger> logger;
        {
            auto& s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            if (!valid(level) || level == Level::Off || s.threshold == Level::Off || level < s.threshold) return;
            logger = s.logger;
        }
        if (!logger) { fallback(component, message); return; }
        // Runtime calls preserve trace/debug availability in release configurations.
        std::string suffix;
        if (context.device_id >= 0) suffix += " device=" + std::to_string(context.device_id);
        if (!context.operation.empty()) suffix += " operation=" + std::string(context.operation);
        if (context.source_index != std::numeric_limits<std::size_t>::max())
            suffix += " source=" + std::to_string(context.source_index);
        logger->log(static_cast<spdlog::level::level_enum>(level), "[{}]{} {}", component, suffix, message);
    } catch (...) { sinkError(); }
}

void setLevel(Level level) noexcept {
    if (!valid(level)) return;
    try {
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.threshold = level;
        if (s.logger) s.logger->set_level(static_cast<spdlog::level::level_enum>(level));
    } catch (...) { sinkError(); }
}
void flush() noexcept {
    try { if (auto logger = snapshot()) logger->flush(); }
    catch (...) { sinkError(); }
}
void shutdown() noexcept {
    try {
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        // Serialize teardown with initialization; existing snapshots keep sinks alive.
        if (s.logger) s.logger->flush();
        s.logger.reset();
        s.initialized = false;
        s.threshold = Level::Warn;
    } catch (...) { sinkError(); }
}

Session::Session(const Config& config) {
    auto& s = state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.session_active) throw std::logic_error("Logging session already owned");
        s.session_active = true;
    }
    try { initialize(config); }
    catch (...) {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.session_active = false;
        throw;
    }
}
Session::~Session() {
    shutdown();
    try {
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.session_active = false;
    } catch (...) { sinkError(); }
}

#ifdef SMART_HOME_LOGGING_TESTING
// Private test seam; never exposed through the public logging header.
void installTestSink(std::shared_ptr<spdlog::sinks::sink> sink) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    errorTimer().store(-1);
    s.logger = std::make_shared<spdlog::logger>("test", std::move(sink));
    s.logger->set_level(static_cast<spdlog::level::level_enum>(s.threshold));
    s.logger->set_error_handler([](const std::string&) { sinkError(); });
}
#endif
} // namespace smart_home::logging
