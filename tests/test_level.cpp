#include <cmath>
#include <tuple>

#include "core/build_config.h"
#include "doctest.h"
#include "world/level.h"

using namespace aldoria;

namespace {

const char* kMinimalLevel = R"({
  "id": "t", "name": "Test",
  "player_spawn": [1, 0, 2],
  "ground": { "size": [40, 40], "color": [10, 20, 30] },
  "boxes": [ { "center": [0, 0.5, 0], "size": [1, 1, 1], "color": [255, 0, 0], "tag": "crate" } ]
})";

Level levelWithBox(Vector3 center, Vector3 size) {
    Level l;
    LevelBox b;
    b.center = center;
    b.size = size;
    l.boxes.push_back(b);
    return l;
}

}  // namespace

TEST_CASE("Level wird aus JSON geladen") {
    LevelLoadResult r = loadLevelFromJson(kMinimalLevel);
    REQUIRE(r.level.has_value());
    CHECK(r.level->id == "t");
    CHECK(r.level->playerSpawn.z == doctest::Approx(2.0f));
    CHECK(r.level->groundSize.x == doctest::Approx(40.0f));
    CHECK(r.level->groundColor.g == 20);
    REQUIRE(r.level->boxes.size() == 1);
    CHECK(r.level->boxes[0].tag == "crate");
    CHECK(r.level->boxes[0].solid);
}

TEST_CASE("Ungültiges JSON liefert einen Fehler statt eines Absturzes") {
    LevelLoadResult r = loadLevelFromJson("{ das ist kein json");
    CHECK_FALSE(r.level.has_value());
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("Box ohne size wird mit Feldnamen abgelehnt") {
    LevelLoadResult r = loadLevelFromJson(R"({ "boxes": [ { "center": [0,0,0] } ] })");
    REQUIRE_FALSE(r.level.has_value());
    CHECK(r.error.find("boxes[0]") != std::string::npos);
}

TEST_CASE("Box mit falscher Größe wird abgelehnt") {
    LevelLoadResult r = loadLevelFromJson(R"({ "boxes": [ { "center": [0,0,0], "size": [1,0,1] } ] })");
    CHECK_FALSE(r.level.has_value());
    r = loadLevelFromJson(R"({ "boxes": [ { "center": [0,0], "size": [1,1,1] } ] })");
    CHECK_FALSE(r.level.has_value());
}

TEST_CASE("Fehlende Datei liefert eine lesbare Meldung") {
    LevelLoadResult r = loadLevelFromFile("gibt/es/nicht.json");
    REQUIRE_FALSE(r.level.has_value());
    CHECK(r.error.find("nicht gefunden") != std::string::npos);
}

TEST_CASE("Das mitgelieferte Testlevel ist gültig") {
    LevelLoadResult r = loadLevelFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/levels/test_level.json");
    REQUIRE_MESSAGE(r.level.has_value(), r.error);
    CHECK(r.level->boxes.size() > 5);
}

TEST_CASE("Wand schiebt den Spieler heraus") {
    Level l = levelWithBox({0, 1, 0}, {2, 2, 2});  // Wand von x -1 bis 1
    Vector3 pos{0.9f, 0.0f, 0.0f};
    l.resolveHorizontal(pos, 0.4f, 1.8f);
    CHECK(pos.x >= 1.4f - 0.001f);
}

TEST_CASE("Spieler im Inneren einer Box wird auf der kürzesten Seite herausgeschoben") {
    Level l = levelWithBox({0, 1, 0}, {2, 2, 2});
    Vector3 pos{0.8f, 0.0f, 0.0f};
    l.resolveHorizontal(pos, 0.4f, 1.8f);
    CHECK(pos.x == doctest::Approx(1.4f));
}

TEST_CASE("Niedrige Kanten sind Stufen und keine Wände") {
    Level l = levelWithBox({0, 0.15f, 0}, {2, 0.3f, 2});
    Vector3 pos{0.0f, 0.0f, 0.0f};
    l.resolveHorizontal(pos, 0.4f, 1.8f);
    CHECK(pos.x == doctest::Approx(0.0f));
    CHECK(l.groundHeight(pos, 0.4f, 0.0f) == doctest::Approx(0.3f));
}

TEST_CASE("Hohe Kisten tragen nur, wenn man darüber ist") {
    Level l = levelWithBox({0, 0.5f, 0}, {1, 1, 1});
    Vector3 onCrate{0.0f, 0.0f, 0.0f};
    CHECK(l.groundHeight(onCrate, 0.4f, 0.0f) == doctest::Approx(0.0f));  // Fuß am Boden: Kiste ist Wand
    CHECK(l.groundHeight(onCrate, 0.4f, 1.0f) == doctest::Approx(1.0f));  // Fuß auf Kistenhöhe: Kiste trägt
    Vector3 beside{3.0f, 0.0f, 0.0f};
    CHECK(l.groundHeight(beside, 0.4f, 1.0f) == doctest::Approx(0.0f));
}

TEST_CASE("Levelgrenze hält den Spieler im Feld") {
    Level l;
    l.groundSize = {10, 10};
    Vector3 pos{100.0f, 0.0f, -100.0f};
    l.resolveHorizontal(pos, 0.5f, 1.8f);
    CHECK(pos.x == doctest::Approx(4.5f));
    CHECK(pos.z == doctest::Approx(-4.5f));
}

namespace {
// Rampe 4 breit (x), 6 lang (z), 1,5 hoch, steigt nach +Z, steht auf dem Boden
Level levelWithRamp() {
    Level l;
    LevelRamp r;
    r.center = {0, 0.75f, 0};
    r.size = {4, 1.5f, 6};
    r.rise = RampDir::PlusZ;
    l.ramps.push_back(r);
    return l;
}
}  // namespace

TEST_CASE("Rampe: Oberflächenhöhe steigt entlang der Richtung") {
    Level l = levelWithRamp();
    const LevelRamp& r = l.ramps[0];
    CHECK(r.surfaceHeight(0, -3) == doctest::Approx(0.0f));
    CHECK(r.surfaceHeight(0, 0) == doctest::Approx(0.75f));
    CHECK(r.surfaceHeight(0, 3) == doctest::Approx(1.5f));
    CHECK(r.surfaceHeight(0, 100) == doctest::Approx(1.5f));  // außerhalb: geklemmt
    CHECK(r.surfaceHeight(0, -100) == doctest::Approx(0.0f));
}

TEST_CASE("Rampe: Steigung in alle vier Richtungen") {
    for (auto [dir, x, z] : {std::tuple{RampDir::PlusX, 1.5f, 0.0f}, std::tuple{RampDir::MinusX, -1.5f, 0.0f},
                             std::tuple{RampDir::PlusZ, 0.0f, 1.5f}, std::tuple{RampDir::MinusZ, 0.0f, -1.5f}}) {
        LevelRamp r;
        r.center = {0, 0.5f, 0};
        r.size = {3, 1, 3};
        r.rise = dir;
        CHECK(r.surfaceHeight(x, z) == doctest::Approx(1.0f));   // hohe Kante
        CHECK(r.surfaceHeight(-x, -z) == doctest::Approx(0.0f));  // tiefe Kante
    }
}

TEST_CASE("Rampe: man läuft von unten hinauf, die Höhe folgt der Oberfläche") {
    Level l = levelWithRamp();
    CHECK(l.groundHeight({0, 0, -2.5f}, 0.4f, 0.0f) == doctest::Approx(0.125f));  // unteres Ende: Stufe
    CHECK(l.groundHeight({0, 0, 0}, 0.4f, 0.0f) == doctest::Approx(0.0f));         // Mitte ist vom Boden aus zu hoch
    CHECK(l.groundHeight({0, 0, 0}, 0.4f, 0.75f) == doctest::Approx(0.75f));       // wer schon oben ist, steht
    CHECK(l.groundHeight({3.0f, 0, 0}, 0.4f, 0.75f) == doctest::Approx(0.0f));     // daneben ist Boden
}

TEST_CASE("Rampe: unteres Ende ist frei, die hohe Seite und die Flanken sind Wände") {
    Level l = levelWithRamp();
    Vector3 entry{0, 0, -3.2f};
    l.resolveHorizontal(entry, 0.4f, 1.8f);
    CHECK(entry.z == doctest::Approx(-3.2f));  // nicht geschoben

    Vector3 side{2.3f, 0, 1.0f};  // seitlich an der hohen Hälfte
    l.resolveHorizontal(side, 0.4f, 1.8f);
    CHECK(side.x >= 2.4f - 0.001f);

    Vector3 inside{0.0f, 0, 0.2f};  // im Inneren, vom Boden aus
    l.resolveHorizontal(inside, 0.4f, 1.8f);
    CHECK(std::fabs(inside.x) >= 2.4f - 0.001f);
}

TEST_CASE("Rampe: Strahlen treffen die Schräge, gehen aber über sie hinweg, wenn sie darüber liegen") {
    Level l = levelWithRamp();
    float hit = 0;
    REQUIRE(l.raycast({0, 6.5f, 0}, {0, -1, 0}, 20.0f, hit));
    CHECK(hit == doctest::Approx(5.75f));                                   // Oberfläche in der Mitte: 0,75

    REQUIRE(l.raycast({5, 0.5f, 0}, {-1, 0, 0}, 20.0f, hit));               // gegen die Flanke
    CHECK(hit == doctest::Approx(3.0f));

    CHECK_FALSE(l.raycast({5, 1.0f, 0}, {-1, 0, 0}, 20.0f, hit));           // knapp über der Schräge
}

TEST_CASE("Level-JSON: Rampen und Gegner werden gelesen") {
    LevelLoadResult r = loadLevelFromJson(R"({
        "ramps": [ { "center": [0, 1, 0], "size": [2, 2, 4], "rise": "-x" } ],
        "enemies": [ { "type": "forest_imp", "position": [1, 0, 2] } ]
    })");
    REQUIRE(r.level.has_value());
    REQUIRE(r.level->ramps.size() == 1);
    CHECK(r.level->ramps[0].rise == RampDir::MinusX);
    REQUIRE(r.level->enemySpawns.size() == 1);
    CHECK(r.level->enemySpawns[0].type == "forest_imp");
    CHECK(r.level->enemySpawns[0].position.z == doctest::Approx(2.0f));
}

TEST_CASE("Level-JSON: falsche Rampenrichtung und Gegner ohne Typ werden abgelehnt") {
    LevelLoadResult r = loadLevelFromJson(R"({ "ramps": [ { "center": [0,0,0], "size": [1,1,1], "rise": "oben" } ] })");
    REQUIRE_FALSE(r.level.has_value());
    CHECK(r.error.find("ramps[0].rise") != std::string::npos);

    r = loadLevelFromJson(R"({ "enemies": [ { "position": [0,0,0] } ] })");
    REQUIRE_FALSE(r.level.has_value());
    CHECK(r.error.find("enemies[0]") != std::string::npos);
}

TEST_CASE("Raycast trifft die nächste Box und ignoriert Treffer dahinter") {
    Level l = levelWithBox({0, 1, 0}, {2, 2, 2});
    LevelBox far;
    far.center = {0, 1, 10};
    far.size = {2, 2, 2};
    l.boxes.push_back(far);

    float hit = 0;
    REQUIRE(l.raycast({0, 1, -5}, {0, 0, 1}, 50.0f, hit));
    CHECK(hit == doctest::Approx(4.0f));

    CHECK_FALSE(l.raycast({0, 1, -5}, {0, 0, 1}, 2.0f, hit));   // zu kurz
    CHECK_FALSE(l.raycast({5, 1, -5}, {0, 0, 1}, 50.0f, hit));  // daneben
}
