#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace aldoria {

// Was die Karte pro Raum wissen muss: Name, Typ und wohin die Türen führen (Himmelsrichtung -> Zielraum).
struct MapDoorInfo {
    std::string side;        // north, south, east, west
    std::string targetRoom;
};

struct MapRoomInfo {
    std::string id, name, type;
    std::vector<MapDoorInfo> doors;
};

struct MapRoom {
    std::string id, name, type;
    int gx = 0, gy = 0;      // Rasterzelle; Norden ist oben (gy wird nach oben kleiner)
};

struct MapLink {
    int a = 0, b = 0;        // Indizes in DungeonMap::rooms
};

struct DungeonMap {
    std::vector<MapRoom> rooms;
    std::vector<MapLink> links;

    int indexOf(const std::string& id) const;
    bool empty() const { return rooms.empty(); }
};

// Ordnet die Räume ab dem Startraum anhand der Türrichtungen auf einem Raster an. Belegte Zellen werden umgangen.
DungeonMap layoutDungeonMap(const std::string& startRoom, const std::map<std::string, MapRoomInfo>& infos);

// Liest die nötigen Angaben aus data/rooms/*.json und legt die Karte an (leer, wenn der Startraum fehlt).
DungeonMap loadDungeonMap(const std::string& startRoom);

// Zeichnet die Karte als Vollbild-Ansicht. Besuchte Räume und ihre unbekannten Nachbarn (als "?") sind sichtbar.
void drawDungeonMap(const DungeonMap& map, const std::set<std::string>& visited, const std::string& currentRoom,
                    const std::string& dungeonName, float time);

}  // namespace aldoria
