#include "core/dungeon_state.h"

namespace aldoria {
namespace {

json toArray(const std::set<std::string>& s) {
    json a = json::array();
    for (const std::string& v : s) a.push_back(v);
    return a;
}

std::set<std::string> readSet(const json& j, const char* key) {
    std::set<std::string> out;
    if (auto it = j.find(key); it != j.end() && it->is_array()) {
        for (const auto& v : *it) {
            if (v.is_string()) out.insert(v.get<std::string>());
        }
    }
    return out;
}

}  // namespace

json DungeonState::toJson() const {
    json j;
    j["version"] = kSaveVersion;
    j["dungeon"] = dungeonId;
    j["current_room"] = currentRoom;
    j["rest_room"] = restRoom;
    j["rest_position"] = {restPosition.x, restPosition.y, restPosition.z};
    j["visited"] = toArray(visitedRooms);
    j["cleared"] = toArray(clearedRooms);
    j["opened_doors"] = toArray(openedDoors);
    j["collected"] = toArray(collected);
    j["latched_switches"] = toArray(latchedSwitches);
    j["defeated_bosses"] = toArray(defeatedBosses);
    j["abilities"] = toArray(abilities);
    j["segen"] = segen;
    j["small_keys"] = smallKeys;
    j["boss_key"] = bossKey;
    j["shards"] = shards;
    j["heart_containers"] = heartContainers;
    j["flask_max"] = flaskMax;
    j["flask_charges"] = flaskCharges;
    j["deaths"] = deaths;
    j["play_time"] = playTime;
    return j;
}

// Fehlende oder falsch getypte Felder behalten ihre Vorgabe, damit ältere Spielstände weiter laden
DungeonState DungeonState::fromJson(const json& j) {
    DungeonState s;
    if (!j.is_object()) return s;
    s.dungeonId = j.value("dungeon", s.dungeonId);
    s.currentRoom = j.value("current_room", s.currentRoom);
    s.restRoom = j.value("rest_room", s.restRoom);
    if (auto it = j.find("rest_position"); it != j.end() && it->is_array() && it->size() == 3) {
        s.restPosition = {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()};
    }
    s.visitedRooms = readSet(j, "visited");
    s.clearedRooms = readSet(j, "cleared");
    s.openedDoors = readSet(j, "opened_doors");
    s.collected = readSet(j, "collected");
    s.latchedSwitches = readSet(j, "latched_switches");
    s.defeatedBosses = readSet(j, "defeated_bosses");
    s.abilities = readSet(j, "abilities");
    if (auto it = j.find("segen"); it != j.end() && it->is_array()) {
        for (const auto& v : *it) {
            if (v.is_string()) s.segen.push_back(v.get<std::string>());
        }
    }
    s.smallKeys = j.value("small_keys", s.smallKeys);
    s.bossKey = j.value("boss_key", s.bossKey);
    s.shards = j.value("shards", s.shards);
    s.heartContainers = j.value("heart_containers", s.heartContainers);
    s.flaskMax = j.value("flask_max", s.flaskMax);
    s.flaskCharges = j.value("flask_charges", s.flaskCharges);
    s.deaths = j.value("deaths", s.deaths);
    s.playTime = j.value("play_time", s.playTime);
    return s;
}

}  // namespace aldoria
