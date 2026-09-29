#pragma once

#include <set>
#include <string>
#include <vector>

#include "core/json.h"
#include "raylib.h"

namespace aldoria {

// Alles, was sich der Dungeon über den Spieler merkt und was gespeichert wird.
struct DungeonState {
    static constexpr int kSaveVersion = 1;

    std::string dungeonId;
    std::string currentRoom;
    std::string restRoom;       // Raum des letzten Ruheplatzes (dort geht es nach dem Tod weiter)
    Vector3 restPosition{0, 0, 0};

    std::set<std::string> visitedRooms;
    std::set<std::string> clearedRooms;     // Räume, deren Gegner erledigt sind
    std::set<std::string> openedDoors;      // "raum:tür", dauerhaft geöffnete Schlüsseltüren
    std::set<std::string> collected;        // "raum:pickup", schon eingesammelt
    std::set<std::string> latchedSwitches;  // "raum:schalter", dauerhaft aktiv
    std::set<std::string> defeatedBosses;
    std::set<std::string> abilities;        // z. B. "dash"
    std::vector<std::string> segen;         // gewählte Upgrades in der Reihenfolge der Wahl

    int smallKeys = 0;
    bool bossKey = false;
    int shards = 0;
    int heartContainers = 0;   // je +10 maximale Lebenspunkte
    int flaskMax = 3;
    int flaskCharges = 3;
    int deaths = 0;
    double playTime = 0.0;

    bool hasAbility(const std::string& a) const { return abilities.count(a) > 0; }
    bool isCleared(const std::string& room) const { return clearedRooms.count(room) > 0; }

    static std::string key(const std::string& room, const std::string& id) { return room + ":" + id; }

    json toJson() const;
    static DungeonState fromJson(const json& j);
};

}  // namespace aldoria
