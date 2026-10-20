#include "core/Diagnostics.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>

namespace loblox::diag {
namespace {

constexpr std::uintmax_t kMaxLogBytes = 1024u * 1024u;
std::atomic<unsigned long long> currentFrame{0};
thread_local std::string frameCorrelation;

struct State {
    std::mutex mutex;
    std::string path;
    std::ofstream output;
    bool openFailed = false;
};

State& GetState() {
    static State state;
    return state;
}

std::string DefaultPath() {
    namespace fs = std::filesystem;
    const char* stateHome = std::getenv("XDG_STATE_HOME");
    if (stateHome != nullptr && stateHome[0] == '/') {
        return (fs::path(stateHome) / "loblox/loblox.jsonl").string();
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return (fs::path(home) / ".local/state/loblox/loblox.jsonl").string();
    }
    std::error_code error;
    const fs::path temporary = fs::temp_directory_path(error);
    return ((error ? fs::path("/tmp") : temporary) / "loblox/loblox.jsonl").string();
}

std::string Timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const time_t seconds = system_clock::to_time_t(now);
    const auto millis = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm utc{};
    gmtime_r(&seconds, &utc);
    char date[32] = {};
    std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc);
    char result[48] = {};
    std::snprintf(result, sizeof(result), "%s.%03lldZ", date,
                  static_cast<long long>(millis));
    return result;
}

bool OpenLocked(State& state) {
    if (state.output.is_open()) return true;
    if (state.path.empty()) state.path = DefaultPath();
    std::error_code error;
    const std::filesystem::path path(state.path);
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            if (!state.openFailed) {
                std::fprintf(stderr, "diagnostico: nao foi possivel criar '%s': %s\n",
                             parent.string().c_str(), error.message().c_str());
                state.openFailed = true;
            }
            return false;
        }
    }
    state.output.open(path, std::ios::app | std::ios::binary);
    if (!state.output) {
        if (!state.openFailed) {
            std::fprintf(stderr, "diagnostico: nao foi possivel abrir '%s'\n",
                         state.path.c_str());
            state.openFailed = true;
        }
        return false;
    }
    state.openFailed = false;
    return true;
}

void RotateLocked(State& state) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(state.path, error);
    if (error || size < kMaxLogBytes) return;
    state.output.close();
    const std::filesystem::path backup = std::filesystem::path(state.path).string() + ".1";
    std::filesystem::remove(backup, error);
    error.clear();
    std::filesystem::rename(state.path, backup, error);
    state.openFailed = false;
}

}  // namespace

const char* LevelName(Level level) {
    switch (level) {
        case Level::Debug: return "debug";
        case Level::Info: return "info";
        case Level::Warning: return "warn";
        case Level::Error: return "error";
    }
    return "info";
}

void configure(const std::string& path) {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.output.close();
    state.path = path.empty() ? DefaultPath() : path;
    state.openFailed = false;
}

void setFrame(unsigned long long frame) {
    currentFrame.store(frame, std::memory_order_relaxed);
    frameCorrelation.clear();
}

std::string newCorrelationId() {
    static std::atomic<unsigned long long> counter{0};
    using namespace std::chrono;
    const auto millis =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return "corr-" + std::to_string(millis) + "-" +
           std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

void log(const Event& event) {
    nlohmann::json record;
    try {
        record["timestamp"] = Timestamp();
        record["level"] = LevelName(event.level);
        record["subsystem"] = event.subsystem;
        record["event"] = event.name;
        record["frame"] = currentFrame.load(std::memory_order_relaxed);
        if (event.correlationId.empty() && frameCorrelation.empty()) frameCorrelation = newCorrelationId();
        record["correlation_id"] = event.correlationId.empty() ? frameCorrelation : event.correlationId;
        if (event.entityId != kInvalidInstanceId) record["entity_id"] = event.entityId;
        if (!event.file.empty()) record["file"] = event.file;
        if (event.durationMs >= 0.0) record["duration_ms"] = event.durationMs;
        if (!event.result.empty()) record["result"] = event.result;
        if (!event.error.empty()) record["error"] = event.error;
        if (!event.context.empty()) record["context"] = event.context;
    } catch (...) {
        return;
    }
    const std::string line = record.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!OpenLocked(state)) return;
    RotateLocked(state);
    if (!state.output.is_open()) {
        state.openFailed = false;
        if (!OpenLocked(state)) return;
    }
    state.output << line << '\n';
    state.output.flush();
    if (!state.output) {
        state.output.close();
        state.openFailed = true;
    }
}

}  // namespace loblox::diag
