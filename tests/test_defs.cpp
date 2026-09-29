#include "combat/weapon_def.h"
#include "core/build_config.h"
#include "doctest.h"
#include "entities/enemy_def.h"

using namespace aldoria;

TEST_CASE("Waffe: Winkel werden von Grad in Radiant umgerechnet, fehlende Felder behalten Vorgaben") {
    json j = json::parse(R"({
        "id": "test", "name": "Testschwert", "combo_window": 0.6,
        "attacks": [ { "name": "X", "damage": 20, "half_angle_deg": 90, "sweep_from_deg": -90, "sweep_to_deg": 90 } ]
    })");
    WeaponDef w = WeaponDef::fromJson(j);
    REQUIRE(w.valid());
    CHECK(w.comboWindow == doctest::Approx(0.6f));
    CHECK(w.attacks[0].damage == doctest::Approx(20.0f));
    CHECK(w.attacks[0].halfAngle == doctest::Approx(1.5707963f).epsilon(0.001));
    CHECK(w.attacks[0].sweepFrom == doctest::Approx(-1.5707963f).epsilon(0.001));
    CHECK(w.attacks[0].windup == doctest::Approx(AttackDef{}.windup));
}

TEST_CASE("Waffe ohne Angriffe ist ungültig") {
    CHECK_FALSE(WeaponDef::fromJson(json::parse(R"({ "id": "leer" })")).valid());
    CHECK_FALSE(WeaponDef::loadFromFile("gibt/es/nicht.json").valid());
}

TEST_CASE("Das mitgelieferte Schwert ist gültig und hat eine dreistufige Kombo") {
    WeaponDef w = WeaponDef::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/weapons/sword.json");
    REQUIRE(w.valid());
    CHECK(w.attacks.size() == 3);
    for (const AttackDef& a : w.attacks) {
        CHECK(a.damage > 0.0f);
        CHECK(a.total() > 0.2f);
    }
    // Die letzte Stufe ist ein Rundumschlag
    CHECK(w.attacks.back().halfAngle >= 3.1f);
}

TEST_CASE("Gegner: Werte aus JSON, fehlende Felder behalten Vorgaben") {
    EnemyDef d = EnemyDef::fromJson(json::parse(R"({ "id": "x", "max_health": 55, "color": [10, 20, 30] })"));
    CHECK(d.id == "x");
    CHECK(d.maxHealth == doctest::Approx(55.0f));
    CHECK(d.color.g == 20);
    CHECK(d.telegraph == doctest::Approx(EnemyDef{}.telegraph));
}

TEST_CASE("Der mitgelieferte Waldkobold hat eine faire Vorwarnung") {
    auto d = EnemyDef::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/enemies/forest_imp.json");
    REQUIRE(d.has_value());
    CHECK(d->id == "forest_imp");
    // Die Vorwarnung muss länger sein als die Unverwundbarkeit der Ausweichrolle nutzbar ist, sonst ist sie unfair
    CHECK(d->telegraph >= 0.4f);
    CHECK(d->attackRecovery >= 0.5f);
}

TEST_CASE("Fehlende Gegnerdatei liefert kein Ergebnis") {
    CHECK_FALSE(EnemyDef::loadFromFile("gibt/es/nicht.json").has_value());
}
