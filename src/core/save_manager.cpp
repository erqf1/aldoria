#include "core/save_manager.h"

#include <filesystem>
#include <fstream>

#include "core/file_util.h"
#include "core/log.h"

namespace aldoria {
namespace fs = std::filesystem;

std::string SaveManager::pathFor(const std::string& slot) const {
    return (fs::path(directory_) / (slot + ".json")).string();
}

bool SaveManager::exists(const std::string& slot) const {
    std::error_code ec;
    return fs::exists(pathFor(slot), ec);
}

bool SaveManager::remove(const std::string& slot) const {
    std::error_code ec;
    fs::remove(pathFor(slot), ec);
    fs::remove(pathFor(slot) + ".bak", ec);
    return !ec;
}

bool SaveManager::save(const std::string& slot, const DungeonState& state, std::string& error) const {
    std::error_code ec;
    fs::create_directories(directory_, ec);
    if (ec) {
        error = "Speicherordner konnte nicht angelegt werden: " + ec.message();
        return false;
    }

    const std::string target = pathFor(slot);
    const std::string temp = target + ".tmp";
    const std::string text = state.toJson().dump(2);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Temporäre Datei konnte nicht geschrieben werden: " + temp;
            return false;
        }
        out << text;
        out.flush();
        if (!out) {
            error = "Schreiben ist fehlgeschlagen: " + temp;
            fs::remove(temp, ec);
            return false;
        }
    }

    // Prüfen, dass die Temp-Datei wirklich lesbar ist, bevor sie den alten Stand ersetzt
    std::string parseError;
    if (!loadJsonFile(temp, parseError)) {
        error = "Kontrolle der geschriebenen Datei fehlgeschlagen: " + parseError;
        fs::remove(temp, ec);
        return false;
    }

    if (fs::exists(target, ec)) fs::copy_file(target, target + ".bak", fs::copy_options::overwrite_existing, ec);
    fs::rename(temp, target, ec);
    if (ec) {
        error = "Umbenennen fehlgeschlagen: " + ec.message();
        return false;
    }
    Log::info(LogCategory::Save, "Spielstand '{}' gespeichert", slot);
    return true;
}

std::optional<DungeonState> SaveManager::load(const std::string& slot, std::string& error) const {
    for (const std::string& path : {pathFor(slot), pathFor(slot) + ".bak"}) {
        std::string err;
        auto j = loadJsonFile(path, err);
        if (j && j->is_object()) {
            if (path != pathFor(slot)) Log::warn(LogCategory::Save, "Hauptdatei unlesbar, nutze Backup: {}", path);
            return DungeonState::fromJson(*j);
        }
        if (path == pathFor(slot)) error = err;
    }
    return std::nullopt;
}

}  // namespace aldoria
