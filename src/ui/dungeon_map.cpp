#include "ui/dungeon_map.h"

#include <algorithm>
#include <cmath>
#include <queue>

#include "core/file_util.h"
#include "core/paths.h"
#include "raylib.h"

namespace aldoria {
namespace {

std::pair<int, int> sideDirection(const std::string& side) {
    if (side == "north") return {0, -1};
    if (side == "south") return {0, 1};
    if (side == "east") return {1, 0};
    if (side == "west") return {-1, 0};
    return {0, 0};
}

std::string roomOfTarget(const std::string& target) {
    size_t colon = target.find(':');
    return colon == std::string::npos ? target : target.substr(0, colon);
}

Color typeColor(const std::string& type) {
    if (type == "arena") return {196, 84, 76, 255};
    if (type == "boss") return {170, 96, 220, 255};
    if (type == "rest") return {96, 190, 118, 255};
    if (type == "platform") return {92, 150, 220, 255};
    if (type == "puzzle") return {88, 200, 210, 255};
    if (type == "treasure") return {236, 200, 90, 255};
    if (type == "end") return {240, 240, 240, 255};
    return {150, 154, 168, 255};
}

}  // namespace

int DungeonMap::indexOf(const std::string& id) const {
    for (size_t i = 0; i < rooms.size(); i++) {
        if (rooms[i].id == id) return (int)i;
    }
    return -1;
}

DungeonMap layoutDungeonMap(const std::string& startRoom, const std::map<std::string, MapRoomInfo>& infos) {
    DungeonMap map;
    auto startIt = infos.find(startRoom);
    if (startIt == infos.end()) return map;

    std::map<std::pair<int, int>, int> occupied;
    auto place = [&](const MapRoomInfo& info, int gx, int gy) {
        MapRoom r;
        r.id = info.id;
        r.name = info.name;
        r.type = info.type;
        r.gx = gx;
        r.gy = gy;
        map.rooms.push_back(r);
        occupied[{gx, gy}] = (int)map.rooms.size() - 1;
        return (int)map.rooms.size() - 1;
    };

    place(startIt->second, 0, 0);
    std::queue<int> todo;
    todo.push(0);
    std::set<std::pair<int, int>> linked;
    while (!todo.empty()) {
        int cur = todo.front();
        todo.pop();
        const MapRoomInfo& info = infos.at(map.rooms[(size_t)cur].id);
        for (const MapDoorInfo& d : info.doors) {
            if (d.targetRoom.empty()) continue;
            auto targetIt = infos.find(d.targetRoom);
            if (targetIt == infos.end()) continue;
            int other = map.indexOf(d.targetRoom);
            if (other < 0) {
                auto [dx, dy] = sideDirection(d.side);
                int baseX = map.rooms[(size_t)cur].gx + dx, baseY = map.rooms[(size_t)cur].gy + dy;
                int gx = baseX, gy = baseY;
                // Zelle belegt: die nächste freie Zelle in der Nähe suchen
                for (int radius = 1; occupied.count({gx, gy}) > 0 && radius < 8; radius++) {
                    bool found = false;
                    for (int oy = -radius; oy <= radius && !found; oy++) {
                        for (int ox = -radius; ox <= radius && !found; ox++) {
                            if (std::max(std::abs(ox), std::abs(oy)) != radius) continue;
                            if (occupied.count({baseX + ox, baseY + oy}) == 0) {
                                gx = baseX + ox;
                                gy = baseY + oy;
                                found = true;
                            }
                        }
                    }
                }
                other = place(targetIt->second, gx, gy);
                todo.push(other);
            }
            std::pair<int, int> key{std::min(cur, other), std::max(cur, other)};
            if (cur != other && linked.insert(key).second) map.links.push_back({key.first, key.second});
        }
    }
    return map;
}

DungeonMap loadDungeonMap(const std::string& startRoom) {
    std::map<std::string, MapRoomInfo> infos;
    std::queue<std::string> todo;
    todo.push(startRoom);
    while (!todo.empty()) {
        std::string id = todo.front();
        todo.pop();
        if (infos.count(id) > 0) continue;
        std::string error;
        auto j = loadJsonFile(dataPath("rooms/" + id + ".json"), error);
        if (!j || !j->is_object()) continue;
        MapRoomInfo info;
        info.id = id;
        info.name = j->value("name", id);
        info.type = j->value("type", std::string());
        if (auto it = j->find("doors"); it != j->end() && it->is_array()) {
            for (const auto& d : *it) {
                if (!d.is_object()) continue;
                MapDoorInfo di;
                di.side = d.value("side", std::string());
                di.targetRoom = roomOfTarget(d.value("target", std::string()));
                if (!di.targetRoom.empty()) todo.push(di.targetRoom);
                info.doors.push_back(std::move(di));
            }
        }
        infos.emplace(id, std::move(info));
    }
    return layoutDungeonMap(startRoom, infos);
}

void drawDungeonMap(const DungeonMap& map, const std::set<std::string>& visited, const std::string& currentRoom,
                    const std::string& dungeonName, float time) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{6, 8, 16, 244});

    auto centered = [&](const std::string& t, int y, int size, Color c) {
        int w = MeasureText(t.c_str(), size);
        DrawText(t.c_str(), sw / 2 - w / 2 + 2, y + 2, size, Color{0, 0, 0, 200});
        DrawText(t.c_str(), sw / 2 - w / 2, y, size, c);
    };
    centered("Karte: " + dungeonName, 24, 34, Color{250, 226, 150, 255});

    if (map.empty()) {
        centered("Für diesen Ort gibt es keine Karte.", sh / 2, 22, WHITE);
        return;
    }

    // Sichtbar: besuchte Räume und deren Nachbarn (unbekannte Räume als "?")
    std::vector<bool> known(map.rooms.size(), false), seen(map.rooms.size(), false);
    for (size_t i = 0; i < map.rooms.size(); i++) known[i] = visited.count(map.rooms[i].id) > 0 || map.rooms[i].id == currentRoom;
    for (size_t i = 0; i < map.rooms.size(); i++) seen[i] = known[i];
    for (const MapLink& l : map.links) {
        if (known[(size_t)l.a]) seen[(size_t)l.b] = true;
        if (known[(size_t)l.b]) seen[(size_t)l.a] = true;
    }

    int minX = 1 << 20, maxX = -(1 << 20), minY = 1 << 20, maxY = -(1 << 20);
    for (size_t i = 0; i < map.rooms.size(); i++) {
        if (!seen[i]) continue;
        minX = std::min(minX, map.rooms[i].gx);
        maxX = std::max(maxX, map.rooms[i].gx);
        minY = std::min(minY, map.rooms[i].gy);
        maxY = std::max(maxY, map.rooms[i].gy);
    }
    if (minX > maxX) return;
    int cols = maxX - minX + 1, rows = maxY - minY + 1;
    float areaX = sw * 0.12f, areaY = 90.0f, areaW = sw * 0.76f, areaH = std::max(120.0f, (float)sh - 90.0f - 120.0f);
    float cellW = std::min(96.0f, areaW / (float)cols), cellH = std::min(64.0f, areaH / (float)rows);
    float boxW = cellW * 0.62f, boxH = cellH * 0.6f;
    float originX = areaX + (areaW - cellW * (float)cols) * 0.5f, originY = areaY + (areaH - cellH * (float)rows) * 0.5f;

    auto center = [&](size_t i) {
        return Vector2{originX + ((float)(map.rooms[i].gx - minX) + 0.5f) * cellW, originY + ((float)(map.rooms[i].gy - minY) + 0.5f) * cellH};
    };

    for (const MapLink& l : map.links) {
        if (!seen[(size_t)l.a] || !seen[(size_t)l.b] || (!known[(size_t)l.a] && !known[(size_t)l.b])) continue;
        DrawLineEx(center((size_t)l.a), center((size_t)l.b), 3.0f, Color{120, 126, 150, 255});
    }
    for (size_t i = 0; i < map.rooms.size(); i++) {
        if (!seen[i]) continue;
        Vector2 c = center(i);
        Rectangle r{c.x - boxW * 0.5f, c.y - boxH * 0.5f, boxW, boxH};
        if (known[i]) {
            DrawRectangleRec(r, typeColor(map.rooms[i].type));
            DrawRectangleLinesEx(r, 2.0f, Color{20, 22, 34, 255});
        } else {
            DrawRectangleRec(r, Color{40, 44, 62, 255});
            DrawRectangleLinesEx(r, 2.0f, Color{100, 106, 130, 255});
            int w = MeasureText("?", 20);
            DrawText("?", (int)(c.x - (float)w / 2.0f), (int)(c.y - 10), 20, Color{170, 176, 200, 255});
        }
        if (map.rooms[i].id == currentRoom) {
            float pulse = 0.5f + 0.5f * std::sin(time * 4.0f);
            Rectangle o{r.x - 4, r.y - 4, r.width + 8, r.height + 8};
            DrawRectangleLinesEx(o, 3.0f, Color{255, 255, 255, (unsigned char)(150.0f + 100.0f * pulse)});
        }
    }

    // Name des aktuellen Raums und Legende
    for (const MapRoom& r : map.rooms) {
        if (r.id == currentRoom) centered("Du bist hier: " + r.name, sh - 104, 22, WHITE);
    }
    struct Legend {
        const char* type;
        const char* label;
    };
    static const Legend legend[] = {{"arena", "Kampf"},   {"boss", "Boss"},         {"rest", "Ruheplatz"}, {"platform", "Sprung"},
                                    {"puzzle", "Rätsel"}, {"treasure", "Schatz"}, {"hub", "Weg"}};
    int total = 0;
    for (const Legend& l : legend) total += 28 + MeasureText(l.label, 16) + 18;
    int x = sw / 2 - total / 2, y = sh - 66;
    for (const Legend& l : legend) {
        DrawRectangle(x, y + 2, 18, 14, typeColor(l.type));
        DrawText(l.label, x + 26, y, 16, Color{206, 210, 228, 255});
        x += 28 + MeasureText(l.label, 16) + 18;
    }
    centered("M oder Esc: schließen", sh - 34, 16, Color{160, 166, 190, 255});
}

}  // namespace aldoria
