#pragma once

#include <optional>
#include <string>

#include "core/dungeon_state.h"

namespace aldoria {

// Speichert Spielstände als JSON. Geschrieben wird atomar (erst in eine Temp-Datei, dann umbenennen),
// der vorherige Stand bleibt als .bak erhalten. Ein gültiger Spielstand wird nie mit leeren Daten überschrieben.
class SaveManager {
public:
    explicit SaveManager(std::string directory) : directory_(std::move(directory)) {}

    bool save(const std::string& slot, const DungeonState& state, std::string& error) const;
    // Lädt den Slot; ist die Hauptdatei beschädigt, wird das Backup versucht.
    std::optional<DungeonState> load(const std::string& slot, std::string& error) const;
    bool exists(const std::string& slot) const;
    bool remove(const std::string& slot) const;

    std::string pathFor(const std::string& slot) const;

private:
    std::string directory_;
};

}  // namespace aldoria
