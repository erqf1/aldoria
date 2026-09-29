#include <cmath>
#include <filesystem>
#include <map>
#include <queue>
#include <set>
#include <vector>

#include "core/build_config.h"
#include "core/dungeon_def.h"
#include "doctest.h"
#include "entities/boss.h"
#include "entities/enemy_def.h"
#include "world/level.h"

using namespace aldoria;
namespace fs = std::filesystem;

namespace {

const std::string kData = ALDORIA_DEV_DATA_DIR;

std::map<std::string, Level> loadAllRooms() {
    std::map<std::string, Level> rooms;
    for (const auto& entry : fs::directory_iterator(kData + "/rooms")) {
        if (entry.path().extension() != ".json") continue;
        LevelLoadResult r = loadLevelFromFile(entry.path().string());
        INFO("Raumdatei: " << entry.path().filename().string());
        REQUIRE_MESSAGE(r.level.has_value(), r.error);
        REQUIRE_MESSAGE(r.level->id == entry.path().stem().string(), "Die Raum-ID muss dem Dateinamen entsprechen");
        rooms.emplace(r.level->id, std::move(*r.level));
    }
    return rooms;
}

std::vector<DungeonDef> loadAllDungeons() {
    std::vector<DungeonDef> out;
    for (const auto& entry : fs::directory_iterator(kData + "/dungeons")) {
        if (entry.path().extension() != ".json") continue;
        std::string error;
        auto def = DungeonDef::loadFromFile(entry.path().string(), error);
        INFO("Dungeon-Datei: " << entry.path().filename().string());
        REQUIRE_MESSAGE(def.has_value(), error);
        REQUIRE_MESSAGE(def->id == entry.path().stem().string(), "Die Dungeon-ID muss dem Dateinamen entsprechen");
        out.push_back(*def);
    }
    return out;
}

// Alle Räume, die man vom Startraum eines Dungeons durch Türen erreicht
std::set<std::string> reachableRooms(const DungeonDef& def, const std::map<std::string, Level>& rooms) {
    std::set<std::string> seen{def.startRoom};
    std::queue<std::string> todo;
    todo.push(def.startRoom);
    while (!todo.empty()) {
        std::string cur = todo.front();
        todo.pop();
        auto it = rooms.find(cur);
        if (it == rooms.end()) continue;
        for (const LevelDoor& d : it->second.doors) {
            if (!d.targetRoom.empty() && seen.insert(d.targetRoom).second) todo.push(d.targetRoom);
        }
    }
    return seen;
}

}  // namespace

TEST_CASE("Dungeon-Definitionen sind gültig und zeigen auf vorhandene Räume") {
    auto dungeons = loadAllDungeons();
    REQUIRE(dungeons.size() >= 2);
    auto rooms = loadAllRooms();
    std::set<std::string> ids;
    for (const DungeonDef& d : dungeons) ids.insert(d.id);
    CHECK(ids.count("wurzelhallen") == 1);
    for (const DungeonDef& d : dungeons) {
        INFO("Dungeon: " << d.id);
        CHECK(rooms.count(d.startRoom) == 1);
        CHECK(rooms.count(d.finalRoom) == 1);
        if (!d.nextDungeon.empty()) CHECK_MESSAGE(ids.count(d.nextDungeon) == 1, "Folge-Dungeon existiert nicht: " << d.nextDungeon);
        CHECK(d.nextDungeon != d.id);
    }
}

TEST_CASE("Die Kette der Dungeons hat kein Ende ohne Ziel und keinen Kreis") {
    auto dungeons = loadAllDungeons();
    std::map<std::string, DungeonDef> byId;
    for (const DungeonDef& d : dungeons) byId.emplace(d.id, d);
    std::set<std::string> seen;
    std::string cur = "wurzelhallen";
    while (!cur.empty()) {
        REQUIRE_MESSAGE(byId.count(cur) == 1, "Dungeon fehlt: " << cur);
        REQUIRE_MESSAGE(seen.insert(cur).second, "Kreis in der Dungeon-Kette bei " << cur);
        cur = byId.at(cur).nextDungeon;
    }
    CHECK(seen.size() == dungeons.size());  // kein Dungeon hängt lose herum
}

TEST_CASE("Alle Räume laden, Türen führen in existierende Räume und Türen") {
    auto rooms = loadAllRooms();
    REQUIRE(rooms.size() >= 20);
    for (const auto& [id, level] : rooms) {
        INFO("Raum: " << id);
        for (const LevelDoor& d : level.doors) {
            INFO("Tür: " << d.id);
            if (d.targetRoom.empty()) continue;
            REQUIRE_MESSAGE(rooms.count(d.targetRoom) == 1, "Zielraum existiert nicht: " << d.targetRoom);
            const Level& target = rooms.at(d.targetRoom);
            const LevelDoor* back = target.findDoor(d.targetDoor);
            REQUIRE_MESSAGE(back != nullptr, "Zieltür existiert nicht: " << d.targetDoor);
            // Die Gegentür führt hierher zurück, damit man wieder zurückgehen kann
            CHECK_MESSAGE(back->targetRoom == id, "Die Zieltür führt nicht zurück in diesen Raum");
        }
    }
}

TEST_CASE("Alle Räume sind von genau einem Dungeon-Start aus erreichbar, jedes Ende ist erreichbar") {
    auto dungeons = loadAllDungeons();
    auto rooms = loadAllRooms();

    std::map<std::string, std::string> owner;  // Raum -> Dungeon
    for (const DungeonDef& def : dungeons) {
        INFO("Dungeon: " << def.id);
        std::set<std::string> seen = reachableRooms(def, rooms);
        CHECK_MESSAGE(seen.count(def.finalRoom) == 1, "Der letzte Raum ist nicht erreichbar");
        for (const std::string& room : seen) {
            auto [it, inserted] = owner.emplace(room, def.id);
            CHECK_MESSAGE(inserted, "Raum " << room << " ist aus zwei Dungeons erreichbar (" << it->second << " und " << def.id << ")");
        }
    }
    for (const auto& [id, level] : rooms) {
        if (id == "title_backdrop" || id == "dev_playground" || id == "arena_endless") continue;
        CHECK_MESSAGE(owner.count(id) == 1, "Nicht erreichbar: " << id);
    }
}

TEST_CASE("Alle verwendeten Gegner, Bosse und Aufsammelarten existieren") {
    auto rooms = loadAllRooms();
    const std::set<std::string> pickupTypes = {"small_key", "boss_key", "shard", "heart", "flask", "potion", "item_dash", "item_double_jump"};
    for (const auto& [id, level] : rooms) {
        INFO("Raum: " << id);
        auto checkEnemy = [&](const std::string& type) {
            CHECK_MESSAGE(EnemyDef::loadFromFile(kData + "/enemies/" + type + ".json").has_value(), "Gegner fehlt: " << type);
        };
        for (const EnemySpawn& s : level.enemySpawns) checkEnemy(s.type);
        if (level.encounter) {
            for (const auto& wave : level.encounter->waves)
                for (const WaveEntry& e : wave) checkEnemy(e.type);
        }
        if (level.boss) {
            CHECK_MESSAGE(BossDef::loadFromFile(kData + "/enemies/" + level.boss->type + ".json").has_value(), "Boss fehlt");
        }
        std::set<std::string> ids;
        for (const LevelPickup& p : level.pickups) {
            CHECK_MESSAGE(pickupTypes.count(p.type) == 1, "Unbekannte Aufsammelart: " << p.type);
            CHECK_MESSAGE(ids.insert(p.id).second, "Doppelte Pickup-ID: " << p.id);
        }
        std::set<std::string> doorIds;
        for (const LevelDoor& d : level.doors) CHECK_MESSAGE(doorIds.insert(d.id).second, "Doppelte Tür-ID: " << d.id);
        // Türen, die von Schaltern abhängen, brauchen den Schalter im Raum
        for (const LevelDoor& d : level.doors) {
            if (d.kind != DoorKind::Switch) continue;
            // "a+b" verlangt mehrere Schalter
            size_t start = 0;
            while (start <= d.switchId.size()) {
                size_t plus = d.switchId.find('+', start);
                std::string one = d.switchId.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
                bool found = false;
                for (const LevelSwitch& s : level.switches) found = found || s.id == one;
                CHECK_MESSAGE(found, "Schalter fehlt für Tür " << d.id << ": " << one);
                if (plus == std::string::npos) break;
                start = plus + 1;
            }
        }
    }
}

TEST_CASE("Genug Schlüssel für alle verschlossenen Türen jedes Dungeons, und es gibt einen Boss-Schlüssel") {
    auto dungeons = loadAllDungeons();
    auto rooms = loadAllRooms();
    for (const DungeonDef& def : dungeons) {
        INFO("Dungeon: " << def.id);
        int keyDoors = 0, keys = 0, bossDoors = 0, bossKeys = 0;
        for (const std::string& id : reachableRooms(def, rooms)) {
            const Level& level = rooms.at(id);
            for (const LevelDoor& d : level.doors) {
                if (d.kind != DoorKind::Key) continue;
                if (d.keyType == "boss_key") bossDoors++;
                else keyDoors++;
            }
            for (const LevelPickup& p : level.pickups) {
                if (p.type == "small_key") keys++;
                if (p.type == "boss_key") bossKeys++;
            }
        }
        CHECK(keys >= keyDoors);
        CHECK(bossKeys >= bossDoors);
        CHECK(bossDoors >= 1);
    }
}

TEST_CASE("Jeder Raum hat einen Startpunkt im begehbaren Bereich und Ankunftspunkte in den Grenzen") {
    auto rooms = loadAllRooms();
    for (const auto& [id, level] : rooms) {
        INFO("Raum: " << id);
        float hx = level.groundSize.x * 0.5f, hz = level.groundSize.y * 0.5f;
        CHECK(std::fabs(level.playerSpawn.x) < hx);
        CHECK(std::fabs(level.playerSpawn.z) < hz);
        for (const LevelDoor& d : level.doors) {
            bool inside = std::fabs(d.arrival.x) < hx && std::fabs(d.arrival.z) < hz;
            CHECK_MESSAGE(inside, "Ankunft liegt außerhalb: " << d.id);
        }
    }
}

TEST_CASE("Arenen haben Wellen und Startpunkte, Bossräume einen Boss und eine Ausgangstür") {
    auto rooms = loadAllRooms();
    for (const auto& [id, level] : rooms) {
        INFO("Raum: " << id);
        if (id == "arena_endless") {
            CHECK(level.arenaSpawnPoints.size() >= 4);
            CHECK(!level.encounter.has_value());  // die Wellen kommen vom ArenaDirector
        } else if (level.type == "arena" && id != "dev_playground") {
            REQUIRE(level.encounter.has_value());
            CHECK(!level.encounter->waves.empty());
            CHECK(!level.encounter->spawnPoints.empty());
        }
        if (level.type == "boss") {
            REQUIRE(level.boss.has_value());
            bool hasExit = false;
            for (const LevelDoor& d : level.doors) hasExit = hasExit || d.kind == DoorKind::BossDefeated;
            CHECK(hasExit);
        }
    }
}

TEST_CASE("Spawnpunkte von Gegnern liegen nicht in Wänden") {
    auto rooms = loadAllRooms();
    for (const auto& [id, level] : rooms) {
        INFO("Raum: " << id);
        auto check = [&](Vector3 p, const std::string& what) {
            // Wenn man an dieser Stelle steht, darf man nicht weit hinausgeschoben werden
            Vector3 q = p;
            level.resolveHorizontal(q, 0.45f, 1.1f);
            bool free = std::fabs(q.x - p.x) < 0.5f && std::fabs(q.z - p.z) < 0.5f;
            CHECK_MESSAGE(free, "Steckt in einem Hindernis: " << what);
        };
        for (const EnemySpawn& s : level.enemySpawns) check(s.position, s.type);
        if (level.encounter) {
            for (const Vector3& p : level.encounter->spawnPoints) check(p, "Arena-Startpunkt");
        }
        if (level.boss) check(level.boss->position, "Boss");
        if (id != "title_backdrop") check(level.playerSpawn, "Spielerstart");  // der Titelhintergrund wird nie betreten
    }
}

namespace {

struct FlatRect {
    float minX, maxX, minZ, maxZ;
};

bool rectContains(const FlatRect& r, float x, float z, float margin) {
    return x >= r.minX - margin && x <= r.maxX + margin && z >= r.minZ - margin && z <= r.maxZ + margin;
}

float rectGap(const FlatRect& a, const FlatRect& b) {
    float dx = std::max(0.0f, std::max(a.minX, b.minX) - std::min(a.maxX, b.maxX));
    float dz = std::max(0.0f, std::max(a.minZ, b.minZ) - std::min(a.maxZ, b.maxZ));
    return std::sqrt(dx * dx + dz * dz);
}

// Stehflächen auf Bodenhöhe: Inseln (Boxen, deren Oberseite bei y = 0 liegt) und alle Positionen der Plattformen
std::vector<FlatRect> standingRects(const Level& level) {
    std::vector<FlatRect> rects;
    for (const LevelBox& b : level.boxes) {
        if (!b.solid || std::fabs(b.maxY()) > 0.06f || b.size.y < 0.2f) continue;
        rects.push_back({b.minX(), b.maxX(), b.minZ(), b.maxZ()});
    }
    for (const LevelPlatform& p : level.platforms) {
        std::vector<Vector3> centers = p.path;
        if (centers.empty()) centers.push_back(level.boxes[(size_t)p.boxIndex].center);
        for (const Vector3& c : centers) rects.push_back({c.x - p.size.x * 0.5f, c.x + p.size.x * 0.5f, c.z - p.size.z * 0.5f, c.z + p.size.z * 0.5f});
    }
    return rects;
}

// Welche Punkte kommen ausgehend vom Spawn über Lücken bis `maxGap` Meter zu Fuß/Sprung? Gibt die nicht erreichbaren zurück.
std::vector<std::string> unreachablePoints(const Level& level, const std::vector<std::pair<std::string, Vector3>>& points, float maxGap) {
    std::vector<FlatRect> rects = standingRects(level);
    std::vector<bool> reached(rects.size(), false);
    std::queue<size_t> todo;
    for (size_t i = 0; i < rects.size(); i++) {
        if (rectContains(rects[i], level.playerSpawn.x, level.playerSpawn.z, 0.3f)) {
            reached[i] = true;
            todo.push(i);
        }
    }
    while (!todo.empty()) {
        size_t cur = todo.front();
        todo.pop();
        for (size_t i = 0; i < rects.size(); i++) {
            if (!reached[i] && rectGap(rects[cur], rects[i]) <= maxGap) {
                reached[i] = true;
                todo.push(i);
            }
        }
    }
    std::vector<std::string> bad;
    for (const auto& [name, p] : points) {
        bool ok = false;
        for (size_t i = 0; i < rects.size() && !ok; i++) ok = reached[i] && rectContains(rects[i], p.x, p.z, 0.3f);
        if (!ok) bad.push_back(name);
    }
    return bad;
}

}  // namespace

TEST_CASE("Räume ohne Boden: alles liegt auf Inseln oder Plattformen und ist über Lücken erreichbar") {
    auto rooms = loadAllRooms();
    // Hier braucht man Dash oder Doppelsprung (Lücken bis ca. 7 m); überall sonst reichen Sprint-Sprünge (bis ca. 5,4 m)
    const std::set<std::string> needsAbility = {"wh08_wind", "ht04_luecken", "ht08_windbruecke"};
    int pitRooms = 0;
    for (const auto& [id, level] : rooms) {
        if (level.hasGround) continue;
        pitRooms++;
        INFO("Raum: " << id);
        std::vector<std::pair<std::string, Vector3>> all, essential;
        all.push_back({"Spielerstart", level.playerSpawn});
        for (const LevelDoor& d : level.doors) {
            if (d.kind == DoorKind::Sealed) continue;
            all.push_back({"Tür " + d.id, d.arrival});
            essential.push_back({"Tür " + d.id, d.arrival});
        }
        for (const LevelPickup& p : level.pickups) {
            all.push_back({"Aufsammelbares " + p.id, p.position});
            if (p.type == "boss_key" || p.type == "item_dash" || p.type == "item_double_jump") essential.push_back({"Aufsammelbares " + p.id, p.position});
        }
        for (const EnemySpawn& e : level.enemySpawns) all.push_back({"Gegner " + e.type, e.position});
        for (const std::string& name : unreachablePoints(level, all, 7.5f)) FAIL_CHECK("Nicht erreichbar oder ohne Boden (auch mit Dash oder Doppelsprung): " << name);
        if (needsAbility.count(id) == 0) {
            // Ohne Fähigkeit: Ausgänge und wichtige Gegenstände müssen erreichbar bleiben
            for (const std::string& name : unreachablePoints(level, essential, 5.4f)) FAIL_CHECK("Ohne Dash und Doppelsprung nicht erreichbar: " << name);
        }
    }
    CHECK(pitRooms >= 4);
}
