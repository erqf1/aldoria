#include "core/paths.h"

#include <cstdlib>
#include <filesystem>

#include "core/build_config.h"
#include "raylib.h"

namespace aldoria {
namespace fs = std::filesystem;

const std::string& dataDir() {
    static const std::string dir = []() -> std::string {
        if (const char* env = std::getenv("ALDORIA_DATA")) {
            if (fs::is_directory(env)) return env;
        }
        // Ein data-Ordner neben der .exe (Weitergabe-Paket) hat Vorrang vor dem Quellbaum des Entwicklungs-Builds
        fs::path beside = fs::path(GetApplicationDirectory()) / "data";
        if (fs::is_directory(beside)) return beside.string();
        if (fs::is_directory(ALDORIA_DEV_DATA_DIR)) return ALDORIA_DEV_DATA_DIR;
        return beside.string();
    }();
    return dir;
}

std::string dataPath(const std::string& relative) { return dataDir() + "/" + relative; }

std::string savesDir() {
    if (const char* env = std::getenv("ALDORIA_SAVES")) return env;
    return (fs::path(GetApplicationDirectory()) / "saves").string();
}

}  // namespace aldoria
