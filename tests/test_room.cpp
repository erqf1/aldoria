#include <cmath>

#include "core/events.h"
#include "doctest.h"
#include "world/room.h"

using namespace aldoria;

namespace {

constexpr float kDt = 1.0f / 60.0f;

Level parse(const char* text) {
    LevelLoadResult r = loadLevelFromJson(text);
    REQUIRE_MESSAGE(r.level.has_value(), r.error);
    return std::move(*r.level);
}

struct Rig {
    DungeonState state;
    EventBus events;
    KinematicBody player;
    std::vector<RoomActor> actors;
    std::unique_ptr<Room> room;

    explicit Rig(const char* levelJson, const char* id = "test") {
        room = std::make_unique<Room>(parse(levelJson), state, id);
        player.position = {0, 0, 0};
        actors.push_back({&player, true, true});
    }

    void run(float seconds, int aliveEnemies = 0) {
        int frames = (int)std::round(seconds / kDt);
        for (int i = 0; i < frames; i++) room->update(kDt, actors, aliveEnemies, events);
    }
};

const char* kShellRoom = R"({
  "id": "shell_room",
  "shell": { "size": [20, 16], "height": 6 },
  "doors": [
    { "id": "n", "side": "north", "x": 0, "width": 4, "kind": "open", "target": "other:s" },
    { "id": "e", "side": "east", "x": 3, "width": 4, "kind": "key", "target": "third:w" }
  ]
})";

}  // namespace

// ---------------------------------------------------------------- Raumhülle und Türen

TEST_CASE("Raumhülle erzeugt Wände mit Lücken für die Türen") {
    Level l = parse(kShellRoom);
    REQUIRE(l.doors.size() == 2);
    CHECK(l.shellSize.x == doctest::Approx(20.0f));

    const LevelDoor* n = l.findDoor("n");
    REQUIRE(n != nullptr);
    CHECK(n->targetRoom == "other");
    CHECK(n->targetDoor == "s");
    CHECK(n->arrival.z == doctest::Approx(-8.0f + 2.5f));  // 2,5 vor der Nordtür im Raum
    CHECK(n->arrivalYaw == doctest::Approx(0.0f));
    CHECK(l.findDoor("e")->arrivalYaw == doctest::Approx(-PI * 0.5f));

    // Außerhalb der Tür ist die Nordwand fest ...
    Vector3 pos{6.0f, 0, -8.2f};  // in der äußeren Hälfte der Wand nahe der Rauminnenseite
    l.resolveHorizontal(pos, 0.4f, 1.8f);
    CHECK(pos.z >= -8.0f + 0.4f - 0.01f);
    // ... und vor der Wand liegt genug Platz im Raum
    Vector3 inside{0, 0, 0};
    l.resolveHorizontal(inside, 0.4f, 1.8f);
    CHECK(inside.z == doctest::Approx(0.0f));
}

TEST_CASE("Ohne Türliste entsteht trotzdem eine geschlossene Raumhülle") {
    Level l = parse(R"({ "shell": { "size": [10, 10], "height": 5 } })");
    CHECK(l.boxes.size() == 4);
    Vector3 pos{0, 0, -5.3f};
    l.resolveHorizontal(pos, 0.4f, 1.8f);
    CHECK(pos.z >= -5.0f + 0.4f - 0.01f);
}

TEST_CASE("Ungültige Raumdaten liefern verständliche Fehler") {
    LevelLoadResult r = loadLevelFromJson(R"({ "doors": [ { "id": "x", "side": "north" } ] })");
    REQUIRE_FALSE(r.level.has_value());
    CHECK(r.error.find("shell") != std::string::npos);

    r = loadLevelFromJson(R"({ "shell": {"size":[10,10]}, "doors": [ { "id": "x", "side": "nordwest" } ] })");
    CHECK_FALSE(r.level.has_value());

    r = loadLevelFromJson(R"({ "doors": [ { "id": "x", "center": [0,0,0], "kind": "geheim" } ] })");
    REQUIRE_FALSE(r.level.has_value());
    CHECK(r.error.find("doors[0].kind") != std::string::npos);

    r = loadLevelFromJson(R"({ "switches": [ { "id": "s", "position": [0,0,0], "kind": "hebel" } ] })");
    CHECK_FALSE(r.level.has_value());

    r = loadLevelFromJson(R"({ "encounter": { "waves": [] } })");
    CHECK_FALSE(r.level.has_value());

    r = loadLevelFromJson(R"({ "platforms": [ { "kind": "moving", "size": [2,1,2] } ] })");
    CHECK_FALSE(r.level.has_value());
}

TEST_CASE("Eine offene Tür führt nach kurzer Schonzeit zum Raumwechsel") {
    Rig r(kShellRoom);
    r.player.position = {0, 0, -8.0f};  // mitten in der Nordtür
    r.run(0.3f);
    CHECK_FALSE(r.room->takeTransition().has_value());  // Schonzeit nach dem Betreten des Raums
    r.run(0.5f);
    auto t = r.room->takeTransition();
    REQUIRE(t.has_value());
    CHECK(t->room == "other");
    CHECK(t->door == "s");
    CHECK_FALSE(r.room->takeTransition().has_value());  // nur einmal
}

TEST_CASE("Die Ankunftsposition liegt nicht im Türbereich") {
    Rig r(kShellRoom);
    r.player.position = r.room->level().findDoor("n")->arrival;
    r.run(1.0f);
    CHECK_FALSE(r.room->takeTransition().has_value());
}

TEST_CASE("Schlüsseltür: bleibt zu, öffnet mit Schlüssel, verbraucht ihn und bleibt danach offen") {
    Rig r(kShellRoom);
    r.player.position = {9.2f, 0, 3.0f};  // direkt vor der Osttür
    r.run(0.2f);
    CHECK_FALSE(r.room->doorOpen(1));

    int unlocked = 0;
    r.events.subscribe<DoorUnlocked>([&](const DoorUnlocked&) { unlocked++; });
    r.state.smallKeys = 1;
    r.run(0.1f);
    CHECK(r.room->doorOpen(1));
    CHECK(r.state.smallKeys == 0);
    CHECK(unlocked == 1);
    CHECK(r.state.openedDoors.count("test:e") == 1);

    // Ein neuer Raum mit demselben Spielstand kennt die geöffnete Tür ohne Schlüssel
    Rig r2(kShellRoom);
    r2.state = r.state;
    Room again(parse(kShellRoom), r2.state, "test");
    std::vector<RoomActor> none;
    again.update(kDt, none, 0, r2.events);
    CHECK(again.doorOpen(1));
}

TEST_CASE("Schlüsseltür ohne Schlüssel in der Nähe öffnet nicht, auch wenn man einen hat, aber weit weg ist") {
    Rig r(kShellRoom);
    r.state.smallKeys = 1;
    r.player.position = {-6.0f, 0, 0};
    r.run(0.3f);
    CHECK_FALSE(r.room->doorOpen(1));
    CHECK(r.state.smallKeys == 1);
}

// ---------------------------------------------------------------- Schalter

const char* kSwitchRoom = R"({
  "id": "switch_room",
  "ground": { "size": [40, 40] },
  "doors": [ { "id": "d", "center": [8, 2, 0], "size": [1, 4, 4], "kind": "switch", "switch": "plate", "target": "x:y" } ],
  "switches": [
    { "id": "plate", "kind": "plate", "position": [0, 0, 0] },
    { "id": "crystal", "kind": "crystal", "position": [-6, 0, 0] }
  ]
})";

TEST_CASE("Druckplatte öffnet die Tür, solange jemand darauf steht") {
    Rig r(kSwitchRoom);
    r.player.position = {10, 0, 10};
    r.run(0.2f);
    CHECK_FALSE(r.room->doorOpen(0));

    r.player.position = {0, 0, 0};  // auf die Platte
    r.run(0.1f);
    CHECK(r.room->switchActive("plate"));
    CHECK(r.room->doorOpen(0));

    r.player.position = {10, 0, 10};
    r.run(0.1f);
    CHECK_FALSE(r.room->switchActive("plate"));
    CHECK_FALSE(r.room->doorOpen(0));
}

TEST_CASE("Ein springender Spieler über der Platte löst sie nicht aus") {
    Rig r(kSwitchRoom);
    r.player.position = {0, 3.0f, 0};
    r.run(0.2f);
    CHECK_FALSE(r.room->switchActive("plate"));
}

TEST_CASE("Kristall: Schwertschlag aktiviert ihn dauerhaft und der Spielstand merkt es sich") {
    Rig r(kSwitchRoom);
    int toggled = 0;
    r.events.subscribe<SwitchToggled>([&](const SwitchToggled&) { toggled++; });
    r.run(0.1f);
    CHECK_FALSE(r.room->switchActive("crystal"));

    // Schlag aus 1,5 m Entfernung Richtung Kristall (x = -6)
    MeleeQuery q{{-4.5f, 0, 0}, -1.5707963f, 2.3f, 1.0f};
    r.room->hitCrystals(q, r.events);
    r.run(0.1f);
    CHECK(r.room->switchActive("crystal"));
    CHECK(toggled == 1);
    CHECK(r.state.latchedSwitches.count("test:crystal") == 1);

    Room again(parse(kSwitchRoom), r.state, "test");
    CHECK(again.switchActive("crystal"));
}

TEST_CASE("Ein Schlag in die falsche Richtung trifft den Kristall nicht") {
    Rig r(kSwitchRoom);
    MeleeQuery q{{-4.5f, 0, 0}, 1.5707963f, 2.3f, 0.8f};  // schaut weg vom Kristall
    r.room->hitCrystals(q, r.events);
    r.run(0.1f);
    CHECK_FALSE(r.room->switchActive("crystal"));
}

TEST_CASE("Funken treffen den Kristall an seiner Position") {
    Rig r(kSwitchRoom);
    r.room->hitCrystalsAt({-6.0f, 1.0f, 0.2f}, 0.2f, r.events);
    r.run(0.1f);
    CHECK(r.room->switchActive("crystal"));
}

// ---------------------------------------------------------------- Plattformen

const char* kPlatformRoom = R"({
  "id": "plat_room",
  "ground": { "enabled": false, "size": [60, 60] },
  "kill_y": -10,
  "platforms": [
    { "id": "mover", "kind": "moving", "size": [4, 0.6, 4], "path": [[0, 1, 0], [10, 1, 0]], "speed": 5, "pause": 0.5 },
    { "id": "brittle", "kind": "crumble", "size": [4, 0.6, 4], "center": [0, 1, 20], "crumble_delay": 0.5, "respawn": 2 }
  ]
})";

TEST_CASE("Bewegliche Plattform nimmt den Spieler mit und wartet an den Enden") {
    Rig r(kPlatformRoom);
    r.player.position = {0, 1.3f, 0};  // Oberseite der Plattform
    r.player.grounded = true;
    r.run(1.0f);
    const LevelBox& box = r.room->level().boxes[r.room->level().platforms[0].boxIndex];
    CHECK(box.center.x == doctest::Approx(5.0f).epsilon(0.02));
    CHECK(r.player.position.x == doctest::Approx(5.0f).epsilon(0.02));
    CHECK(r.player.position.y == doctest::Approx(1.3f));

    r.run(1.2f);  // Ende erreicht (2 s Fahrt) und kurze Pause
    CHECK(box.center.x == doctest::Approx(10.0f).epsilon(0.05));

    r.run(2.0f);  // zurück
    CHECK(box.center.x < 9.0f);
}

TEST_CASE("Wer neben der Plattform steht, wird nicht mitgenommen") {
    Rig r(kPlatformRoom);
    r.player.position = {0, 1.3f, 5.0f};
    r.player.grounded = true;
    r.run(1.0f);
    CHECK(r.player.position.x == doctest::Approx(0.0f));
}

TEST_CASE("Bröckelnde Plattform: hält kurz, fällt, kommt wieder") {
    Rig r(kPlatformRoom);
    int crumbled = 0;
    r.events.subscribe<PlatformCrumbled>([&](const PlatformCrumbled&) { crumbled++; });
    const LevelBox& box = r.room->level().boxes[r.room->level().platforms[1].boxIndex];

    r.run(0.5f);
    CHECK(box.solid);  // ohne Besucher passiert nichts

    r.player.position = {0, 1.3f, 20};
    r.player.grounded = true;
    r.run(0.3f);
    CHECK(box.solid);  // Warnzeit läuft
    r.run(0.4f);
    CHECK_FALSE(box.solid);
    CHECK(crumbled == 1);

    r.player.position = {0, 5.0f, 0};  // runter von der Plattform
    r.run(1.2f);
    CHECK_FALSE(box.visible);
    r.run(2.5f);  // nach der Wartezeit ist sie zurück
    CHECK(box.solid);
    CHECK(box.visible);
    CHECK(box.center.y == doctest::Approx(1.0f));
}

// ---------------------------------------------------------------- Aufsammelbares

const char* kPickupRoom = R"({
  "id": "pickup_room",
  "ground": { "size": [40, 40] },
  "pickups": [
    { "id": "k1", "type": "small_key", "position": [2, 0.6, 0] },
    { "id": "s1", "type": "shard", "position": [-2, 0.6, 0] },
    { "id": "dash", "type": "item_dash", "position": [0, 0.6, 3] }
  ]
})";

TEST_CASE("Aufsammeln: Wirkung, Ereignis und dauerhaft gemerkt") {
    Rig r(kPickupRoom);
    std::vector<std::string> collected;
    r.events.subscribe<PickupCollected>([&](const PickupCollected& e) { collected.push_back(e.type); });
    bool dashUnlocked = false;
    r.events.subscribe<AbilityUnlocked>([&](const AbilityUnlocked& e) { dashUnlocked = e.ability == "dash"; });

    r.player.position = {5, 0, 5};
    r.run(0.1f);
    CHECK(collected.empty());  // zu weit weg

    r.player.position = {2, 0, 0};
    r.run(0.1f);
    CHECK(r.state.smallKeys == 1);
    r.player.position = {-2, 0, 0};
    r.run(0.1f);
    CHECK(r.state.shards == 1);
    r.player.position = {0, 0, 3};
    r.run(0.1f);
    CHECK(r.state.hasAbility("dash"));
    CHECK(dashUnlocked);
    CHECK(collected.size() == 3);

    // Ein zweites Mal an derselben Stelle gibt nichts mehr
    r.player.position = {2, 0, 0};
    r.run(0.3f);
    CHECK(r.state.smallKeys == 1);

    // Nach dem Neuladen des Raums sind sie weg
    Room again(parse(kPickupRoom), r.state, "test");
    Rig r2(kPickupRoom);
    r2.state = r.state;
    r2.room = std::make_unique<Room>(parse(kPickupRoom), r2.state, "test");
    r2.player.position = {2, 0, 0};
    r2.run(0.2f);
    CHECK(r2.state.smallKeys == 1);
    for (const RoomPickup& p : r2.room->pickups()) CHECK(p.collected);
}

TEST_CASE("Ein Pickup weit über oder unter dem Spieler wird nicht eingesammelt") {
    Rig r(kPickupRoom);
    r.player.position = {2, 5.0f, 0};
    r.run(0.2f);
    CHECK(r.state.smallKeys == 0);
}

TEST_CASE("Dynamische Pickups werden nicht gespeichert") {
    Rig r(kPickupRoom);
    r.room->addPickup("health_drop", {6, 0.5f, 6});
    r.player.position = {6, 0, 6};
    r.run(0.2f);
    CHECK(r.state.collected.empty());
}

// ---------------------------------------------------------------- Arena

const char* kArenaRoom = R"({
  "id": "arena_room",
  "type": "arena",
  "shell": { "size": [20, 20], "height": 6 },
  "doors": [
    { "id": "in", "side": "south", "kind": "arena", "target": "prev:out" },
    { "id": "out", "side": "north", "kind": "arena", "target": "next:in" }
  ],
  "encounter": {
    "waves": [ [ { "type": "forest_imp", "count": 2 } ], [ { "type": "forest_imp", "count": 1 } ] ],
    "spawn_points": [ [0, 0, -5], [4, 0, -5] ]
  }
})";

TEST_CASE("Arena: Kampf startet erst nach kurzer Zeit im Raum, Türen schließen sich") {
    Rig r(kArenaRoom);
    r.player.position = {0, 0, 5};
    r.run(0.3f);
    CHECK(r.room->encounterPhase() == EncounterPhase::Waiting);
    CHECK(r.room->doorOpen(0));

    int started = 0;
    r.events.subscribe<EncounterStarted>([&](const EncounterStarted&) { started++; });
    r.run(0.7f);
    CHECK(r.room->encounterPhase() == EncounterPhase::Countdown);
    CHECK(started == 1);
    CHECK_FALSE(r.room->doorOpen(0));
    CHECK_FALSE(r.room->doorOpen(1));
    CHECK(r.room->inCombat());
}

TEST_CASE("Arena: Wellen, Zwischenpause und Abschluss mit einmaliger Belohnungsmeldung") {
    Rig r(kArenaRoom);
    r.player.position = {0, 0, 5};
    int waveEvents = 0, clearedEvents = 0;
    r.events.subscribe<EncounterWaveStarted>([&](const EncounterWaveStarted&) { waveEvents++; });
    r.events.subscribe<EncounterCleared>([&](const EncounterCleared&) { clearedEvents++; });

    r.run(0.9f + 1.7f);  // Betreten und Countdown
    REQUIRE(r.room->encounterPhase() == EncounterPhase::Fighting);
    auto wave1 = r.room->takeSpawnRequests();
    REQUIRE(wave1.size() == 2);
    CHECK(wave1[0].type == "forest_imp");
    CHECK(r.room->takeSpawnRequests().empty());

    r.run(0.5f, /*aliveEnemies=*/2);
    CHECK(r.room->encounterPhase() == EncounterPhase::Fighting);

    r.run(0.1f, 0);  // beide besiegt
    CHECK(r.room->encounterPhase() == EncounterPhase::Between);
    r.run(1.8f, 0);
    CHECK(r.room->encounterPhase() == EncounterPhase::Fighting);
    CHECK(r.room->takeSpawnRequests().size() == 1);
    CHECK(waveEvents == 2);

    CHECK_FALSE(r.room->takeEncounterCleared());
    r.run(0.1f, 0);
    CHECK(r.room->encounterPhase() == EncounterPhase::Cleared);
    CHECK(clearedEvents == 1);
    CHECK(r.room->takeEncounterCleared());
    CHECK_FALSE(r.room->takeEncounterCleared());
    CHECK(r.state.isCleared("test"));
    CHECK(r.room->doorOpen(0));
    CHECK(r.room->doorOpen(1));
}

TEST_CASE("Ein schon gesäuberter Arena-Raum startet keinen Kampf mehr") {
    Rig r(kArenaRoom);
    r.state.clearedRooms.insert("test");
    r.room = std::make_unique<Room>(parse(kArenaRoom), r.state, "test");
    r.player.position = {0, 0, 5};
    r.run(4.0f);
    CHECK(r.room->encounterPhase() == EncounterPhase::Cleared);
    CHECK(r.room->takeSpawnRequests().empty());
    CHECK(r.room->doorOpen(0));
}

TEST_CASE("Mehr Gegner als Startpunkte stehen nicht alle auf demselben Fleck") {
    const char* text = R"({
      "encounter": { "waves": [ [ { "type": "x", "count": 5 } ] ], "spawn_points": [ [0, 0, 0] ] }
    })";
    Rig r(text);
    r.player.position = {0, 0, 0};
    r.run(3.0f);
    auto spawns = r.room->takeSpawnRequests();
    REQUIRE(spawns.size() == 5);
    for (size_t i = 1; i < spawns.size(); i++) {
        float d = std::hypot(spawns[i].position.x - spawns[0].position.x, spawns[i].position.z - spawns[0].position.z);
        CHECK(d > 0.5f);
    }
}

// ---------------------------------------------------------------- Boss und Gefahren

TEST_CASE("Bosstür öffnet sich erst, wenn der Boss besiegt ist") {
    Rig r(R"({
      "doors": [ { "id": "exit", "center": [0, 2, -9], "size": [4, 4, 1], "kind": "boss_defeated", "target": "a:b" } ],
      "boss": { "type": "kobold_king", "position": [0, 0, 0] }
    })");
    r.run(0.2f);
    CHECK_FALSE(r.room->doorOpen(0));
    int defeated = 0;
    r.events.subscribe<BossDefeated>([&](const BossDefeated&) { defeated++; });
    r.room->setBossFight(true);
    r.room->setBossDefeated(r.events);
    r.run(0.1f);
    CHECK(r.room->doorOpen(0));
    CHECK(defeated == 1);
    CHECK(r.state.defeatedBosses.count("kobold_king") == 1);
    CHECK(r.state.isCleared("test"));
}

TEST_CASE("Kampf im Bossraum hält die Arena-Türen geschlossen") {
    Rig r(R"({
      "doors": [ { "id": "in", "center": [0, 2, 9], "size": [4, 4, 1], "kind": "arena", "target": "a:b" } ],
      "boss": { "type": "kobold_king", "position": [0, 0, 0] }
    })");
    r.run(0.2f);
    CHECK(r.room->doorOpen(0));
    r.room->setBossFight(true);
    r.run(0.1f);
    CHECK_FALSE(r.room->doorOpen(0));
}

TEST_CASE("Gefahrenzonen werden nur bei Überlappung gemeldet") {
    Rig r(R"({ "hazards": [ { "kind": "spikes", "center": [0, 0.2, 0], "size": [3, 0.4, 3], "damage": 15 } ] })");
    CHECK(r.room->hazardAt({0, 0, 0}, 0.4f, 1.8f) != nullptr);
    CHECK(r.room->hazardAt({0, 0, 0}, 0.4f, 1.8f)->damage == doctest::Approx(15.0f));
    CHECK(r.room->hazardAt({5, 0, 0}, 0.4f, 1.8f) == nullptr);
    CHECK(r.room->hazardAt({0, 3.0f, 0}, 0.4f, 1.8f) == nullptr);  // darüber gesprungen
}

// ---------------------------------------------------------------- Level ohne Boden

TEST_CASE("Räume ohne Boden: nichts trägt, Absturzhöhe gilt") {
    Level l = parse(R"({ "ground": { "enabled": false, "size": [40, 40] }, "kill_y": -8 })");
    CHECK_FALSE(l.hasGround);
    CHECK(l.killY == doctest::Approx(-8.0f));
    CHECK(l.groundHeight({0, 0, 0}, 0.4f, 0.0f) < -1000.0f);
}

TEST_CASE("Dekoration wird deterministisch gestreut und liegt außerhalb der Hülle") {
    const char* text = R"({
      "shell": { "size": [20, 20] },
      "scatter": [ { "kind": "tree", "count": 30, "seed": 5, "margin": 10 } ]
    })";
    Level a = parse(text), b = parse(text);
    REQUIRE(a.decor.size() == 30);
    CHECK(a.decor[7].position.x == doctest::Approx(b.decor[7].position.x));
    for (const LevelDecor& d : a.decor) {
        CHECK((std::fabs(d.position.x) > 11.0f || std::fabs(d.position.z) > 11.0f));
    }
}
