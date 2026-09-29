#pragma once

#include <deque>
#include <format>
#include <string>

namespace aldoria {

enum class LogCategory { Core, Player, Combat, World, Save, Loading };
enum class LogLevel { Info, Warn, Error };

// Einfaches Logging mit Kategorien. Schreibt in die Log-Datei und merkt sich die letzten Zeilen
// für das Debug-Overlay. Nichts davon läuft pro Frame.
class Log {
public:
    static void openFile(const std::string& path);
    static void write(LogCategory category, LogLevel level, const std::string& message);
    static const std::deque<std::string>& recent();

    template <class... Args>
    static void info(LogCategory c, std::format_string<Args...> fmt, Args&&... args) {
        write(c, LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }
    template <class... Args>
    static void warn(LogCategory c, std::format_string<Args...> fmt, Args&&... args) {
        write(c, LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }
    template <class... Args>
    static void error(LogCategory c, std::format_string<Args...> fmt, Args&&... args) {
        write(c, LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }
};

}  // namespace aldoria
