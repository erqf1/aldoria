#include <cmath>
#include <set>

#include "doctest.h"
#include "entities/boss.h"
#include "world/level.h"

using namespace aldoria;

namespace {

constexpr float kDt = 1.0f / 60.0f;

struct Arena {
    Level level = Level::makeFallback();
    BossDef def;
    Boss boss;
    Vector3 player{0, 0, -10};

    Arena() : def(makeDef()), boss(&def, {0, 0, 0}) {
        SetRandomSeed(7);
        boss.start();
    }

    static BossDef makeDef() {
        BossDef d;
        d.maxHealth = 100000.0f;   // wir wollen viele Angriffe sehen, nicht sterben
        return d;
    }

    // Läuft `seconds` Spielzeit und sammelt alle Ereignisse
    std::vector<BossEvent> run(float seconds, bool* sawCharge = nullptr, Vector3* chargeDir = nullptr) {
        std::vector<BossEvent> all;
        int frames = (int)(seconds / kDt);
        for (int i = 0; i < frames; i++) {
            boss.update(kDt, level, player, true);
            if (sawCharge && boss.chargeHitActive()) {
                if (!*sawCharge && chargeDir) *chargeDir = boss.chargeDirection();   // nur den ersten Ansturm merken
                *sawCharge = true;
            }
            for (const BossEvent& e : boss.takeEvents()) all.push_back(e);
        }
        return all;
    }
};

}  // namespace

TEST_CASE("Boss: Feuerbälle zielen auf die Brusthöhe des Spielers") {
    Arena a;
    auto events = a.run(240.0f);
    int volleys = 0;
    for (const BossEvent& e : events) {
        if (e.type != BossEvent::Type::Volley) continue;
        volleys++;
        CHECK(e.target.y == doctest::Approx(a.player.y + 0.9f).epsilon(0.01));
        CHECK(std::fabs(e.target.x - a.player.x) < 1.0f);
        CHECK(e.position.y < a.def.height * 0.7f);   // nicht mehr über dem Kopf des Spielers abgefeuert
    }
    CHECK(volleys >= 1);
}

TEST_CASE("Boss: Wer nur aus der Ferne wartet, wird angerannt") {
    Arena a;
    bool sawCharge = false;
    Vector3 dir{0, 0, 0};
    a.run(240.0f, &sawCharge, &dir);
    REQUIRE(sawCharge);
    // Der Ansturm läuft ungefähr auf den Spieler zu (der steht bei -z)
    CHECK(dir.z < -0.7f);
}

TEST_CASE("Boss: In Phase 2 kommen Meteore und Spiralfeuer") {
    Arena a;
    a.boss.takeHit(a.def.maxHealth * 0.4f, {0, 0, 0});   // Phase 2
    auto events = a.run(300.0f);
    int meteors = 0;
    std::set<int> spiralAngles;
    for (const BossEvent& e : events) {
        if (e.type == BossEvent::Type::Meteors) {
            meteors++;
            CHECK(e.count >= 1);
            CHECK(e.speed > 0.5f);   // Verzögerung als Vorwarnung
        }
        if (e.type == BossEvent::Type::SpiralShot) spiralAngles.insert((int)std::round(std::atan2(e.direction.x, e.direction.z) * 10.0f));
    }
    CHECK(meteors >= 1);
    CHECK(spiralAngles.size() >= 6);   // dreht sich rund um den Boss
}

TEST_CASE("Boss: Gegen die Wand gerannt wird er benommen und verwundbar") {
    Arena a;
    a.level = Level::makeFallback();
    // Der Spieler steht hinter einer Wand aus Sicht des Bosses, der Ansturm endet an der Wand
    LevelBox wall;
    wall.center = {0, 2, -5};
    wall.size = {30, 4, 1};
    a.level.boxes.push_back(wall);
    bool crashed = false;
    for (const BossEvent& e : a.run(300.0f)) crashed = crashed || e.type == BossEvent::Type::ChargeCrash;
    CHECK(crashed);
}

TEST_CASE("Boss: Funken richten nur einen Teil des Schadens an") {
    BossDef d;
    CHECK(d.sparkResist < 1.0f);
    CHECK(d.sparkResist > 0.0f);
}
