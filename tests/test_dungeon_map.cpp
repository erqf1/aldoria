#include <set>

#include "core/build_config.h"
#include "core/dungeon_def.h"
#include "doctest.h"
#include "ui/dungeon_map.h"

using namespace aldoria;

namespace {

MapRoomInfo room(const std::string& id, const std::string& type, std::vector<MapDoorInfo> doors) {
    MapRoomInfo r;
    r.id = id;
    r.name = id;
    r.type = type;
    r.doors = std::move(doors);
    return r;
}

}  // namespace

TEST_CASE("Karte: Räume folgen den Türrichtungen") {
    std::map<std::string, MapRoomInfo> infos;
    infos["a"] = room("a", "hub", {{"north", "b"}});
    infos["b"] = room("b", "arena", {{"south", "a"}, {"east", "c"}});
    infos["c"] = room("c", "rest", {{"west", "b"}});
    DungeonMap m = layoutDungeonMap("a", infos);
    REQUIRE(m.rooms.size() == 3);
    CHECK(m.rooms[(size_t)m.indexOf("a")].gy == 0);
    CHECK(m.rooms[(size_t)m.indexOf("b")].gy == -1);   // Norden liegt oben
    CHECK(m.rooms[(size_t)m.indexOf("c")].gx == 1);
    CHECK(m.rooms[(size_t)m.indexOf("c")].gy == -1);
    CHECK(m.links.size() == 2);                        // jede Verbindung nur einmal
}

TEST_CASE("Karte: belegte Zellen werden umgangen, unbekannte Ziele ignoriert") {
    std::map<std::string, MapRoomInfo> infos;
    // b und c liegen beide im Norden von a: c muss ausweichen
    infos["a"] = room("a", "hub", {{"north", "b"}, {"north", "c"}, {"west", "fehlt"}});
    infos["b"] = room("b", "hub", {});
    infos["c"] = room("c", "hub", {});
    DungeonMap m = layoutDungeonMap("a", infos);
    REQUIRE(m.rooms.size() == 3);
    std::set<std::pair<int, int>> cells;
    for (const MapRoom& r : m.rooms) cells.insert({r.gx, r.gy});
    CHECK(cells.size() == 3);
    CHECK(layoutDungeonMap("gibt_es_nicht", infos).empty());
}

TEST_CASE("Karte: beide Dungeons ergeben eine vollständige Karte ohne doppelte Zellen") {
    for (const char* dungeon : {"wurzelhallen", "glutschmiede"}) {
        INFO("Dungeon: " << dungeon);
        std::string error;
        auto def = DungeonDef::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/dungeons/" + dungeon + ".json", error);
        REQUIRE_MESSAGE(def.has_value(), error);
        DungeonMap m = loadDungeonMap(def->startRoom);
        REQUIRE(m.rooms.size() >= 10);
        CHECK(m.rooms[0].id == def->startRoom);
        CHECK(m.indexOf(def->finalRoom) >= 0);
        std::set<std::pair<int, int>> cells;
        for (const MapRoom& r : m.rooms) cells.insert({r.gx, r.gy});
        CHECK_MESSAGE(cells.size() == m.rooms.size(), "Zwei Räume liegen auf derselben Kartenzelle");
        CHECK(m.links.size() >= m.rooms.size() - 1);
    }
}
