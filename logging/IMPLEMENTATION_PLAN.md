# spdlog implementation plan

Prepared using Antigravity (`agy -p ... --dangerously-skip-permissions`), then
reviewed against this repository. This document proposes implementation; no
application code or dependencies have been changed.

## 1. Add the dependency and logging module

- Use `find_package(spdlog CONFIG QUIET)` and compiled `spdlog::spdlog`.
- Add an opt-in `SMART_HOME_FETCH_SPDLOG` option, default OFF, for a pinned
  FetchContent fallback. Verify and pin the release during implementation.
  If the package is unavailable and fetching is disabled, provide an actionable
  CMake error. Disable upstream examples, tests, and installation for the fallback.
- Use the imported target's fmt configuration; do not independently redefine
  `SPDLOG_FMT_EXTERNAL` or mix compiled/header-only configurations.
- Create `logging/CMakeLists.txt`, `logging/include/logging/Logging.h`, and
  `logging/src/Logging.cpp`.
- Define `smart_home_logging` and alias `SmartHome::Logging`, matching the other
  modules. Add `add_subdirectory(logging)` before consumers in root CMake.
- Link spdlog PRIVATE because the facade exposes no spdlog/fmt types. Link
  `SmartHome::Logging` PRIVATE in each consumer that actually logs. Keep logging
  includes out of existing public domain headers.

## 2. Keep a small public API

Use namespace `smart_home::logging` with these proposed declarations:

```cpp
enum class Level { Trace, Debug, Info, Warn, Error, Critical, Off };

struct Config {
    Level level = Level::Info;
    std::string file_path = "logs/smart_home.log";
    std::size_t max_file_size = 5 * 1024 * 1024;
    std::size_t max_rotated_files = 3;
};

void initialize(const Config& config);
bool enabled(Level level) noexcept;
void log(Level level, std::string_view component,
         std::string_view message) noexcept;
void setLevel(Level level) noexcept;
void flush() noexcept;
void shutdown() noexcept;
```

An empty file path disables file output. `Off` disables ordinary records.
Use ordinary function calls with runtime filtering so debug and trace remain
available in release builds. Check `enabled()` before expensive message
construction. Do not introduce SPDLOG macros or a custom formatting framework
in the public header. Exceptions from optional message construction must not
replace a business exception at an error-handling boundary.

## 3. Configure output and lifecycle

- Use `stderr_color_sink_mt` and `rotating_file_sink_mt`, with one INFO threshold
  by default. The correct console sink header is
  `<spdlog/sinks/stdout_color_sinks.h>`.
- Start with synchronous logging. Include timestamp, severity, component and
  thread ID; include local device ID, operation ID and source index where useful.
  Keep console color markers out of the file pattern.
- Preserve sensor measurements on `std::cout`.
- Resolve each configuration field independently: CLI > environment > defaults.
  Support `--log-level`, `--log-file`, `--log-max-size-mib`, `--log-max-files`
  and corresponding `SMART_HOME_LOG_*` variables. Accept all levels above,
  case-insensitively. Document units and relative-path behavior.
- Invalid explicit CLI values produce an actionable stderr message and nonzero
  exit before application work. Invalid environment values produce a diagnostic
  and use defaults for that field. Validate positive sizes, archive counts, and
  numeric conversion overflow.
- Initialize once in `main`; an application-owned RAII guard flushes and shuts
  down after business objects are destroyed, including exceptional exits.
  Repeated initialization with identical configuration is a no-op; different
  configuration is rejected without replacing the active logger.
- Before initialization or after shutdown, WARN and higher use a minimal stderr
  fallback; lower levels are suppressed. Libraries must not configure logging
  or shut down spdlog's global registry.
- Synchronize access to logger ownership and runtime levels. Hold a shared
  logger snapshot while emitting; serialize lifecycle operations.

## 4. Define failure behavior

- File creation failure emits one direct stderr diagnostic and continues with
  console logging. If console logger creation fails, use minimal stderr fallback.
- Attach an error handler to the owned logger, rather than changing unrelated
  global loggers. Report runtime sink failures directly to stderr with rate
  limiting, never recursively through the logger.
- Logging, fallback, flush and shutdown must not throw into business code.
  A failed log write must not hide the original exception. Initialization misuse
  and invalid configuration remain explicit caller errors.
- Do not dump credentials or raw payloads at any severity. Physical device
  addresses are omitted by default; sanitize exception text where it may carry
  such values.

## 5. Add events at the correct boundaries

| Level | Project events |
| --- | --- |
| TRACE | Detailed scan/dispatch progress, enabled explicitly |
| DEBUG | Device registration/restoration and command routing details |
| INFO | Startup/shutdown, completed discovery summaries, configuration summary |
| WARN | Handled discovery source failures, unsupported discovered drivers, rejected user commands |
| ERROR | Failed operation/storage action when a caller handles the failure and continues |
| CRITICAL | Fatal unhandled application failure immediately before nonzero exit |

- In `DiscoveryService::scanOnce()`, log WARN inside its existing separate
  `DiscoveryScanError` and `UnsupportedDriverError` catch blocks. Preserve
  `DiscoveryReport` accumulation and the current signature/return value.
- Add INFO/DEBUG success events in `DeviceManager` and relevant orchestration.
- Keep `CommandDispatcher`, validation helpers, registries and storage throwing
  as they currently do. Do not add catches merely to log and rethrow.
- Log a rejected command at its caller's recovery boundary. Classify failures
  by operation and recovery policy, not just `std::invalid_argument` versus
  `std::runtime_error` (the exception classes overlap in existing code).
- Add a top-level catch in `main` for fatal failures. A storage failure that
  propagates there gets one CRITICAL record, not ERROR records at every layer.
  ERROR belongs at a genuine recovery boundary; do not invent one to emit a level.
- Define one owner of each error record so an error already logged by discovery
  is not logged again when its report is displayed.

## 6. Verify behavior

Add a CTest logging executable using the existing test style. Provide a private
test seam for an ostream/memory sink rather than exposing spdlog in public APIs.

- Verify all severity thresholds, OFF, runtime changes, and debug/trace in release.
- Verify configuration precedence and invalid CLI/environment behavior.
- Verify stderr diagnostics do not contaminate measurement stdout.
- Verify lifecycle behavior, no-init fallback, repeated initialization, and flush.
- Verify file initialization failure and a deliberately failing runtime sink do
  not alter business exceptions or recurse.
- Use temporary directories to verify rotation and archive retention. Keep each
  test record below the size threshold; oversized records can exceed that size.
- Verify one record per handled discovery failure and one record for a fatal
  propagated failure. Retain the original nine CTest tests.
- Configure/build, run CTest, and build the `docs` target.

## 7. Document the implementation

Add Russian Doxygen comments to new logging APIs and document configuration,
levels, defaults, rotation and fallback behavior in README. Update `.gitignore`
for the default runtime log directory. `cmake/Docs.cmake` already includes the
`logging` directory; use `cmake --build build --target docs`, not a separate
hand-maintained Doxyfile.

Reference: [official spdlog documentation](https://github.com/gabime/spdlog),
including severity filtering, multithreaded sinks, rotation and error handlers.
