#include "TestUtil.h"
#include "logging/Logging.h"
#include "Testing.h"
#include "discovery/DiscoveryService.h"
#include "discovery/SimulatedDiscoverySource.h"
#include "storage/InMemoryDeviceStorage.h"
#include "registry/DriverFactoryRegistry.h"
#include "registry/DeviceRegistry.h"
#include "device_management/DeviceManager.h"
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <unistd.h>

namespace logging = smart_home::logging;
namespace fs = std::filesystem;
namespace {
struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        auto pattern = (fs::temp_directory_path() / "smart-home-logging-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        auto* result = mkdtemp(buffer.data());
        if (!result) throw std::runtime_error("Cannot create test directory");
        path = result;
    }
    ~TemporaryDirectory() { std::error_code ignored; fs::remove_all(path, ignored); }
};
struct Environment {
    std::vector<std::pair<std::string, std::string>> saved;
    std::vector<std::string> absent;
    Environment() {
        for (const char* name : {"SMART_HOME_LOG_LEVEL", "SMART_HOME_LOG_FILE", "SMART_HOME_LOG_MAX_SIZE_MIB", "SMART_HOME_LOG_MAX_FILES"}) {
            if (const auto* value = std::getenv(name)) saved.emplace_back(name, value);
            else absent.emplace_back(name);
            unsetenv(name);
        }
    }
    ~Environment() {
        for (const auto& pair : saved) setenv(pair.first.c_str(), pair.second.c_str(), 1);
        for (const auto& name : absent) unsetenv(name.c_str());
    }
};
struct CaptureStderr {
    std::FILE* file = std::tmpfile();
    int saved = -1;
    CaptureStderr() {
        if (!file) throw std::runtime_error("tmpfile failed");
        std::fflush(stderr);
        saved = dup(STDERR_FILENO);
        if (saved == -1 || dup2(fileno(file), STDERR_FILENO) == -1)
            throw std::runtime_error("stderr redirect failed");
    }
    ~CaptureStderr() {
        std::fflush(stderr);
        dup2(saved, STDERR_FILENO);
        close(saved);
        std::fclose(file);
    }
    std::string text() {
        std::fflush(stderr);
        std::rewind(file);
        std::string result;
        char buffer[512];
        while (const auto count = std::fread(buffer, 1, sizeof(buffer), file)) result.append(buffer, count);
        return result;
    }
};
class FailingSink : public spdlog::sinks::base_sink<std::mutex> {
    void sink_it_(const spdlog::details::log_msg&) override { throw std::runtime_error("sink failed"); }
    void flush_() override { throw std::runtime_error("flush failed"); }
};
std::size_t occurrences(const std::string& text, const std::string& word) {
    std::size_t count = 0, position = 0;
    while ((position = text.find(word, position)) != std::string::npos) { ++count; position += word.size(); }
    return count;
}
void configuration() {
    Environment environment;
    std::ostringstream warnings;
    const char* defaults[]{"test"};
    const auto initial = logging::resolveConfig(1, defaults, warnings);
    require(initial.level == logging::Level::Info && initial.max_file_size == 5 * 1024 * 1024 && initial.max_rotated_files == 3, "defaults");
    setenv("SMART_HOME_LOG_LEVEL", "bad", 1);
    setenv("SMART_HOME_LOG_MAX_FILES", "-1", 1);
    const char* cli[]{"test", "--log-level=DeBuG", "--log-file", "", "--log-max-size-mib=2"};
    const auto parsed = logging::resolveConfig(5, cli, warnings);
    require(parsed.level == logging::Level::Debug && parsed.file_path.empty() && parsed.max_file_size == 2 * 1024 * 1024, "CLI precedence and empty file");
    require(warnings.str().find("SMART_HOME_LOG_LEVEL") == std::string::npos, "overridden environment ignored");
    require(warnings.str().find("SMART_HOME_LOG_MAX_FILES") != std::string::npos && parsed.max_rotated_files == 3, "invalid environment falls back");
    for (const char* argument : {"--log-level=bad", "--log-max-files=0", "--log-max-files=200001", "--log-max-size-mib=18446744073709551615", "--unknown=1", "--log-file"}) {
        const char* argv[]{"test", argument};
        expectException<std::invalid_argument>([&] { logging::resolveConfig(2, argv, warnings); });
    }
}
void lifecycleAndLevels() {
    logging::shutdown();
    {
        CaptureStderr capture;
        logging::log(logging::Level::Info, "test", "hidden");
        logging::log(logging::Level::Warn, "test", "fallback-warning");
        const auto text = capture.text();
        require(text.find("hidden") == std::string::npos && text.find("fallback-warning") != std::string::npos, "no-init fallback");
    }
    logging::Config config;
    config.file_path.clear();
    for (int field = 0; field < 3; ++field) {
        auto invalid = config;
        if (field == 0) invalid.level = static_cast<logging::Level>(99);
        if (field == 1) invalid.max_file_size = 0;
        if (field == 2) invalid.max_rotated_files = 0;
        bool rejected = false;
        try { logging::initialize(invalid); }
        catch (const std::invalid_argument& error) {
            rejected = true;
            const std::string text(error.what());
            require(text.find(field == 0 ? "level" : field == 1 ? "size" : "Archive") != std::string::npos,
                    "initialization reports the invalid field");
        }
        require(rejected, "invalid configuration rejected before initialization");
    }
    logging::initialize(config);
    logging::initialize(config);
    auto other = config;
    other.level = logging::Level::Warn;
    expectException<std::logic_error>([&] { logging::initialize(other); });
    std::ostringstream output;
    logging::installTestSink(std::make_shared<spdlog::sinks::ostream_sink_mt>(output));
    for (int threshold = 0; threshold <= 6; ++threshold) {
        output.str(""); output.clear();
        logging::setLevel(static_cast<logging::Level>(threshold));
        for (int level = 0; level <= 6; ++level)
            logging::log(static_cast<logging::Level>(level), "test", "level-" + std::to_string(level));
        const auto text = output.str();
        for (int level = 0; level <= 6; ++level)
            require((text.find("level-" + std::to_string(level)) != std::string::npos) == (level >= threshold && level < 6 && threshold < 6), "severity threshold");
    }
    logging::setLevel(logging::Level::Trace);
    output.str(""); output.clear();
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) threads.emplace_back([] {
        for (int i = 0; i < 50; ++i) logging::log(logging::Level::Debug, "thread", "concurrent");
    });
    for (auto& thread : threads) thread.join();
    logging::flush();
    require(occurrences(output.str(), "concurrent") == 200, "concurrent records");
    {
        CaptureStderr capture;
        logging::installTestSink(std::make_shared<FailingSink>());
        logging::log(logging::Level::Error, "test", "failed write");
        logging::log(logging::Level::Error, "test", "failed write again");
        logging::flush();
        require(occurrences(capture.text(), "Log sink failed") == 1, "sink failure fallback is rate limited");
    }
    logging::shutdown();
    require(logging::enabled(logging::Level::Warn) && !logging::enabled(logging::Level::Info), "shutdown fallback");
}
void files() {
    TemporaryDirectory directory;
    logging::Config config;
    config.file_path = (directory.path / "blocking" / "log.txt").string();
    std::ofstream(directory.path / "blocking") << "not a directory";
    {
        CaptureStderr capture;
        logging::initialize(config);
        logging::log(logging::Level::Error, "test", "console-alive");
        logging::shutdown();
        const auto text = capture.text();
        require(text.find("Cannot create log file") != std::string::npos && text.find("console-alive") != std::string::npos, "file failure console fallback");
    }
    config.file_path = (directory.path / "rotation.log").string();
    config.max_file_size = 256;
    config.max_rotated_files = 3;
    config.level = logging::Level::Trace;
    {
        CaptureStderr capture;
        logging::Session session(config);
        expectException<std::logic_error>([&] { logging::Session nested(config); });
        for (int i = 0; i < 60; ++i) logging::log(logging::Level::Trace, "rotation", "small record");
    }
    for (const char* name : {"rotation.log", "rotation.1.log", "rotation.2.log", "rotation.3.log"})
        require(fs::exists(directory.path / name) && fs::file_size(directory.path / name) <= 256, "rotation files and size");
    require(!fs::exists(directory.path / "rotation.4.log"), "archive retention");
}
void boundaries() {
    logging::Config config;
    config.file_path.clear();
    logging::Session session(config);
    std::ostringstream output;
    logging::installTestSink(std::make_shared<spdlog::sinks::ostream_sink_mt>(output));
    smart_home::InMemoryDeviceStorage storage;
    smart_home::DriverFactoryRegistry factories;
    smart_home::DeviceRegistry devices;
    smart_home::DeviceManager manager(storage, factories, devices);
    std::vector<std::unique_ptr<smart_home::IDeviceDiscoverySource>> sources;
    sources.push_back(std::make_unique<smart_home::SimulatedDiscoverySource>(std::vector<smart_home::DiscoveredDevice>{}, true));
    sources.push_back(std::make_unique<smart_home::SimulatedDiscoverySource>(std::vector<smart_home::DiscoveredDevice>{{"PRIVATE-ADDRESS", "missing", "name"}}, false));
    smart_home::DiscoveryService discovery(manager, std::move(sources));
    const auto report = discovery.scanOnce();
    require(report.errors.size() == 2 && occurrences(output.str(), "[warning]") == 2, "one warning per handled failure");
    require(output.str().find("PRIVATE-ADDRESS") == std::string::npos && output.str().find("source=0") != std::string::npos, "safe source context");
    output.str(""); output.clear();
    try { storage.insert("", "driver", "name"); }
    catch (const std::invalid_argument&) { logging::log(logging::Level::Critical, "Main", "Fatal operation failure."); }
    require(occurrences(output.str(), "[critical]") == 1 && output.str().find("[error]") == std::string::npos, "propagated error logged once");
}
} // namespace
int main() {
    configuration();
    lifecycleAndLevels();
    files();
    boundaries();
}
