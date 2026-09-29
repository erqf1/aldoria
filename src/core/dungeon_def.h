#pragma once

#include <optional>
#include <string>

#include "core/json.h"
#include "raylib.h"

namespace aldoria {

// Beschreibung eines Dungeons aus data/dungeons/<id>.json. Die Räume liegen einzeln in data/rooms/.
struct DungeonDef {
    std::string id;
    std::string name;
    std::string startRoom;
    std::string startDoor;        // Ankunftstür im Startraum ("" = player_spawn des Raums)
    std::string finalRoom;        // Erreichen dieses Raums schließt den Dungeon ab
    std::string outro;            // Text im Abschlussbild nach dem Boss
    std::string nextDungeon;      // Dungeon, der danach folgt ("" = letzter Dungeon)
    std::string description;

    static std::optional<DungeonDef> fromJson(const json& j, std::string& error);
    static std::optional<DungeonDef> loadFromFile(const std::string& path, std::string& error);
};

}  // namespace aldoria
