#include "core/log.h"

#include <chrono>
#include <cstdio>

namespace aldoria {
namespace {

constexpr size_t kRecentLines = 12;

std::deque<std::string> g_recent;
std::FILE* g_file = nullptr;
const auto g_start = std::chrono::steady_clock::now();

const char* categoryName(LogCategory c) {
    switch (c) {
        case LogCategory::Core: return "Core";
        case LogCategory::Player: return "Player";
        case LogCategory::Combat: return "Combat";
        case LogCategory::World: return "World";
        case LogCategory::Save: return "Save";
        case LogCategory::Loading: return "Loading";
    }
    return "?";
}

const char* levelName(LogLevel l) {
    switch (l) {
        case LogLevel::Info: return "INFO ";
        case LogLevel::Warn: return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

}  // namespace

void Log::openFile(const std::string& path) {
    if (g_file) std::fclose(g_file);
    g_file = std::fopen(path.c_str(), "w");
}

void Log::write(LogCategory category, LogLevel level, const std::string& message) {
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
    std::string line = std::format("[{:8.2f}] {} [{}] {}", seconds, levelName(level), categoryName(category), message);

    std::fprintf(stderr, "%s\n", line.c_str());
    if (g_file) {
        std::fprintf(g_file, "%s\n", line.c_str());
        std::fflush(g_file);
    }
    g_recent.push_back(line);
    if (g_recent.size() > kRecentLines) g_recent.pop_front();
}

const std::deque<std::string>& Log::recent() { return g_recent; }

}  // namespace aldoria
