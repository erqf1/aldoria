#include "core/dungeon_def.h"

#include "core/file_util.h"

namespace aldoria {

std::optional<DungeonDef> DungeonDef::fromJson(const json& j, std::string& error) {
    if (!j.is_object()) {
        error = "Dungeon-Datei muss ein Objekt sein";
        return std::nullopt;
    }
    DungeonDef d;
    d.id = j.value("id", std::string());
    d.name = j.value("name", d.id);
    d.startRoom = j.value("start_room", std::string());
    d.startDoor = j.value("start_door", std::string());
    d.finalRoom = j.value("final_room", std::string());
    d.nextDungeon = j.value("next_dungeon", std::string());
    d.description = j.value("description", std::string());
    d.outro = j.value("outro", d.description);
    if (d.id.empty()) {
        error = "Dungeon: 'id' fehlt";
        return std::nullopt;
    }
    if (d.startRoom.empty()) {
        error = "Dungeon: 'start_room' fehlt";
        return std::nullopt;
    }
    return d;
}

std::optional<DungeonDef> DungeonDef::loadFromFile(const std::string& path, std::string& error) {
    auto j = loadJsonFile(path, error);
    if (!j) return std::nullopt;
    return fromJson(*j, error);
}

}  // namespace aldoria
