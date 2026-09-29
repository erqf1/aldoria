#include <filesystem>
#include <fstream>

#include "core/save_manager.h"
#include "doctest.h"

using namespace aldoria;
namespace fs = std::filesystem;

namespace {

// Eigener leerer Ordner pro Test
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name) {
        path = fs::temp_directory_path() / ("aldoria_test_" + name);
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

DungeonState sampleState() {
    DungeonState s;
    s.dungeonId = "wurzelhallen";
    s.currentRoom = "r05";
    s.restRoom = "r04";
    s.restPosition = {1.5f, 0, -2.5f};
    s.visitedRooms = {"r01", "r02"};
    s.clearedRooms = {"r02"};
    s.openedDoors = {"r03:door"};
    s.collected = {"r02:k1", "r03:s1"};
    s.latchedSwitches = {"r02:crystal"};
    s.defeatedBosses = {"kobold_king"};
    s.abilities = {"dash"};
    s.segen = {"scharfe_klinge", "flinke_beine"};
    s.smallKeys = 2;
    s.bossKey = true;
    s.shards = 17;
    s.heartContainers = 1;
    s.flaskMax = 4;
    s.flaskCharges = 2;
    s.deaths = 3;
    s.playTime = 1234.5;
    return s;
}

}  // namespace

TEST_CASE("Spielstand übersteht das Speichern als JSON unverändert") {
    DungeonState a = sampleState();
    DungeonState b = DungeonState::fromJson(a.toJson());
    CHECK(b.dungeonId == a.dungeonId);
    CHECK(b.currentRoom == "r05");
    CHECK(b.restPosition.x == doctest::Approx(1.5f));
    CHECK(b.restPosition.z == doctest::Approx(-2.5f));
    CHECK(b.clearedRooms == a.clearedRooms);
    CHECK(b.openedDoors == a.openedDoors);
    CHECK(b.collected == a.collected);
    CHECK(b.latchedSwitches == a.latchedSwitches);
    CHECK(b.defeatedBosses == a.defeatedBosses);
    CHECK(b.abilities == a.abilities);
    CHECK(b.segen == a.segen);
    CHECK(b.smallKeys == 2);
    CHECK(b.bossKey);
    CHECK(b.shards == 17);
    CHECK(b.heartContainers == 1);
    CHECK(b.flaskMax == 4);
    CHECK(b.flaskCharges == 2);
    CHECK(b.deaths == 3);
    CHECK(b.playTime == doctest::Approx(1234.5));
}

TEST_CASE("Fehlende oder falsche Felder im Spielstand fallen auf Vorgaben zurück") {
    DungeonState s = DungeonState::fromJson(json::parse(R"({ "version": 1, "shards": 4, "abilities": "kaputt", "segen": [1, "a"] })"));
    CHECK(s.shards == 4);
    CHECK(s.abilities.empty());
    CHECK(s.segen == std::vector<std::string>{"a"});
    CHECK(s.flaskMax == DungeonState{}.flaskMax);
    // Kein Objekt: leerer Spielstand statt Absturz
    CHECK(DungeonState::fromJson(json::parse("[1,2]")).shards == 0);
}

TEST_CASE("Speichern und Laden über die Festplatte") {
    TempDir dir("save_roundtrip");
    SaveManager sm(dir.path.string());
    CHECK_FALSE(sm.exists("slot1"));

    std::string error;
    REQUIRE(sm.save("slot1", sampleState(), error));
    CHECK(sm.exists("slot1"));
    CHECK_FALSE(fs::exists(sm.pathFor("slot1") + ".tmp"));  // keine Reste

    auto loaded = sm.load("slot1", error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->shards == 17);
}

TEST_CASE("Ein zweites Speichern behält den vorherigen Stand als Backup") {
    TempDir dir("save_backup");
    SaveManager sm(dir.path.string());
    std::string error;

    DungeonState first = sampleState();
    first.shards = 5;
    REQUIRE(sm.save("slot1", first, error));
    DungeonState second = sampleState();
    second.shards = 9;
    REQUIRE(sm.save("slot1", second, error));
    CHECK(fs::exists(sm.pathFor("slot1") + ".bak"));

    CHECK(sm.load("slot1", error)->shards == 9);
}

TEST_CASE("Beschädigte Hauptdatei: das Backup wird geladen statt leerer Daten") {
    TempDir dir("save_corrupt");
    SaveManager sm(dir.path.string());
    std::string error;

    DungeonState first = sampleState();
    first.shards = 5;
    REQUIRE(sm.save("slot1", first, error));
    DungeonState second = sampleState();
    second.shards = 9;
    REQUIRE(sm.save("slot1", second, error));

    {  // Absturz beim Schreiben simulieren
        std::ofstream out(sm.pathFor("slot1"), std::ios::trunc);
        out << "{ halb geschriebe";
    }
    auto loaded = sm.load("slot1", error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->shards == 5);  // Stand des Backups
}

TEST_CASE("Laden ohne Datei liefert eine Fehlermeldung") {
    TempDir dir("save_missing");
    SaveManager sm(dir.path.string());
    std::string error;
    CHECK_FALSE(sm.load("gibt_es_nicht", error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("Löschen entfernt Hauptdatei und Backup") {
    TempDir dir("save_remove");
    SaveManager sm(dir.path.string());
    std::string error;
    REQUIRE(sm.save("slot1", sampleState(), error));
    REQUIRE(sm.save("slot1", sampleState(), error));
    sm.remove("slot1");
    CHECK_FALSE(sm.exists("slot1"));
    CHECK_FALSE(fs::exists(sm.pathFor("slot1") + ".bak"));
}
