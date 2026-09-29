#include <cmath>

#include "combat/player_modifiers.h"
#include "combat/projectile.h"
#include "core/build_config.h"
#include "doctest.h"
#include "render/particles.h"

using namespace aldoria;

namespace {
constexpr float kDt = 1.0f / 60.0f;
}

TEST_CASE("Projektil fliegt geradeaus und läuft nach seiner Lebenszeit ab") {
    Level level = Level::makeFallback();
    ProjectileSystem ps;
    Projectile p;
    p.position = {0, 1, 0};
    p.velocity = {0, 0, 10};
    p.life = 0.5f;
    ps.spawn(p);

    for (int i = 0; i < 15; i++) ps.update(kDt, level);
    REQUIRE(ps.all().size() == 1);
    CHECK(ps.all()[0].position.z == doctest::Approx(2.5f).epsilon(0.02));

    for (int i = 0; i < 30; i++) ps.update(kDt, level);
    CHECK(ps.all().empty());
}

TEST_CASE("Projektil endet an einer Wand und meldet den Einschlagort") {
    Level level = Level::makeFallback();
    LevelBox wall;
    wall.center = {0, 1, 5};
    wall.size = {6, 2, 1};
    level.boxes.push_back(wall);

    ProjectileSystem ps;
    Projectile p;
    p.position = {0, 1, 0};
    p.velocity = {0, 0, 20};
    p.life = 5.0f;
    ps.spawn(p);
    for (int i = 0; i < 30 && !ps.all().empty(); i++) ps.update(kDt, level);

    CHECK(ps.all().empty());
    auto impacts = ps.takeImpacts();
    REQUIRE(impacts.size() == 1);
    CHECK(impacts[0].z == doctest::Approx(4.5f).epsilon(0.1));
    CHECK(ps.takeImpacts().empty());
}

TEST_CASE("Ein schneller Schuss tunnelt nicht durch dünne Wände") {
    Level level = Level::makeFallback();
    LevelBox wall;
    wall.center = {0, 1, 5};
    wall.size = {6, 2, 0.1f};
    level.boxes.push_back(wall);

    ProjectileSystem ps;
    Projectile p;
    p.position = {0, 1, 0};
    p.velocity = {0, 0, 90};  // 1,5 m pro Frame
    ps.spawn(p);
    for (int i = 0; i < 10; i++) ps.update(kDt, level);
    CHECK(ps.all().empty());
}

TEST_CASE("Projektile treffen den Boden") {
    Level level = Level::makeFallback();
    ProjectileSystem ps;
    Projectile p;
    p.position = {0, 1, 0};
    p.velocity = {5, -10, 0};
    ps.spawn(p);
    for (int i = 0; i < 30; i++) ps.update(kDt, level);
    CHECK(ps.all().empty());
    CHECK(ps.takeImpacts().size() == 1);
}

TEST_CASE("Partikel altern und verschwinden, die Anzahl ist begrenzt") {
    Particles pt;
    pt.burst({0, 0, 0}, 20, WHITE, 3.0f, 0.1f, 0.3f);
    CHECK(pt.count() == 20);
    for (int i = 0; i < 60; i++) pt.update(kDt);
    CHECK(pt.count() == 0);

    for (int i = 0; i < 20; i++) pt.burst({0, 0, 0}, 100, WHITE, 3.0f, 0.1f, 5.0f);
    CHECK(pt.count() <= 700);
}

TEST_CASE("Segen: Effekte werden gemultipelt und addiert") {
    SegenLibrary lib = SegenLibrary::fromJson(json::parse(R"({ "segen": [
        { "id": "a", "name": "A", "effects": { "damage_mult": 1.2, "max_health_add": 10 } },
        { "id": "b", "name": "B", "effects": { "damage_mult": 1.5, "max_health_add": 15, "resistance": 0.5 } },
        { "id": "c", "name": "C", "effects": { "resistance": 0.6, "unbekannt": 3 } }
    ] })"));
    REQUIRE(lib.all.size() == 3);
    CHECK(lib.find("b")->name == "B");
    CHECK(lib.find("zzz") == nullptr);
    CHECK(lib.find("c")->effects.size() == 1);  // unbekannter Effekt wird verworfen

    PlayerModifiers m = lib.combine({"a", "b"});
    CHECK(m.damageMult == doctest::Approx(1.8f));
    CHECK(m.maxHealthAdd == doctest::Approx(25.0f));
    CHECK(m.resistance == doctest::Approx(0.5f));

    // Widerstand ist gedeckelt
    m = lib.combine({"b", "c"});
    CHECK(m.resistance == doctest::Approx(0.9f));

    // Unbekannte gewählte Segen (z. B. aus alten Spielständen) werden ignoriert
    m = lib.combine({"a", "gibt_es_nicht"});
    CHECK(m.damageMult == doctest::Approx(1.2f));
}

TEST_CASE("Die mitgelieferten Segen sind gültig und jeder Effekt ist bekannt") {
    SegenLibrary lib = SegenLibrary::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/segen.json");
    REQUIRE(lib.all.size() >= 10);
    for (const SegenDef& s : lib.all) {
        CHECK_FALSE(s.name.empty());
        CHECK_FALSE(s.description.empty());
        CHECK_FALSE(s.effects.empty());
    }
    // Alle zusammen ergeben eine plausible Summe
    std::vector<std::string> all;
    for (const SegenDef& s : lib.all) all.push_back(s.id);
    PlayerModifiers m = lib.combine(all);
    CHECK(m.damageMult > 1.0f);
    CHECK(m.resistance <= 0.9f);
}
