#include <map>

#include "core/arena_director.h"
#include "core/build_config.h"
#include "doctest.h"
#include "entities/enemy_def.h"

using namespace aldoria;

namespace {
constexpr float kDt = 1.0f / 60.0f;

int cost(const std::vector<std::string>& wave) {
    int c = 0;
    for (const auto& t : wave) c += ArenaDirector::enemyCost(t);
    return c;
}
}  // namespace

TEST_CASE("Wellen bleiben im Punktebudget und wachsen") {
    std::mt19937 rng(5);
    int previous = 0;
    for (int w : {1, 3, 6, 12, 25}) {
        auto wave = ArenaDirector::composeWave(w, rng);
        CHECK(!wave.empty());
        CHECK((float)cost(wave) <= ArenaDirector::budgetForWave(w) + 0.001f);
        CHECK(wave.size() <= 14);
        CHECK(cost(wave) >= previous / 2);
        previous = cost(wave);
    }
    std::mt19937 late(1);
    CHECK(cost(ArenaDirector::composeWave(20, late)) > cost(ArenaDirector::composeWave(1, late)));
}

TEST_CASE("Neue Gegnerarten kommen erst in späteren Wellen") {
    std::mt19937 rng(9);
    std::map<std::string, int> seenEarly, seenLate;
    for (int i = 0; i < 60; i++) {
        for (auto& t : ArenaDirector::composeWave(1, rng)) seenEarly[t]++;
        for (auto& t : ArenaDirector::composeWave(9, rng)) seenLate[t]++;
    }
    CHECK(seenEarly.size() == 1);
    CHECK(seenEarly.count("forest_imp") == 1);
    CHECK(seenLate.count("imp_archer") == 1);
    CHECK(seenLate.count("leaper") == 1);
    CHECK(seenLate.count("brute") == 1);
}

TEST_CASE("Ab Welle 8 kommen Gegner der Glutschmiede dazu, und alle Arten haben Datendateien") {
    std::mt19937 rng(11);
    std::map<std::string, int> seenMid, seenHigh;
    for (int i = 0; i < 80; i++) {
        for (auto& t : ArenaDirector::composeWave(7, rng)) seenMid[t]++;
        for (auto& t : ArenaDirector::composeWave(14, rng)) seenHigh[t]++;
    }
    CHECK(seenMid.count("ember_imp") == 0);
    CHECK(seenHigh.count("wind_imp") == 0);  // Sturmgegner erst ab Welle 16
    CHECK(seenHigh.count("ember_imp") == 1);
    CHECK(seenHigh.count("magma_brute") == 1);
    std::map<std::string, int> seenLate;
    for (int i = 0; i < 80; i++)
        for (auto& t : ArenaDirector::composeWave(24, rng)) seenLate[t]++;
    CHECK(seenLate.count("wind_imp") == 1);
    CHECK(seenLate.count("gale_brute") == 1);
    for (const auto& [type, n] : seenLate) {
        CHECK_MESSAGE(EnemyDef::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/enemies/" + type + ".json").has_value(), "Gegner fehlt: " << type);
    }
    for (const auto& [type, n] : seenHigh) {
        CHECK_MESSAGE(EnemyDef::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/enemies/" + type + ".json").has_value(), "Gegner fehlt: " << type);
    }
}

TEST_CASE("Bosswellen wechseln zwischen Kobold-König, Schmiedegolem und Sturmwächter") {
    ArenaDirector d(3);
    std::vector<std::string> bosses;
    ArenaDirector::Update u;
    for (int w = 1; w <= 30; w++) {
        u = {};
        for (int i = 0; i < 1000 && !u.waveStarted; i++) u = d.update(kDt, 0);
        REQUIRE(u.waveStarted);
        if (u.boss) bosses.push_back(*u.boss);
        d.update(kDt, 0);  // Welle beenden
    }
    REQUIRE(bosses.size() == 3);
    CHECK(bosses[0] == "kobold_king");
    CHECK(bosses[1] == "schmiedegolem");
    CHECK(bosses[2] == "sturmwaechter");
}

TEST_CASE("Boss-Rush: jede Welle ist ein Boss, abwechselnd, mit Segen danach") {
    ArenaDirector d(4);
    d.setBossRush(true);
    std::vector<std::string> bosses;
    for (int w = 1; w <= 4; w++) {
        ArenaDirector::Update u;
        for (int i = 0; i < 1000 && !u.waveStarted; i++) u = d.update(kDt, 0);
        REQUIRE(u.waveStarted);
        REQUIRE(u.boss.has_value());
        CHECK(u.spawns.empty());
        bosses.push_back(*u.boss);
        CHECK_FALSE(d.update(kDt, 0, /*bossAlive=*/true).waveCleared);
        auto end = d.update(kDt, 0, false);
        CHECK(end.waveCleared);
        CHECK(end.offerSegen);
    }
    CHECK(bosses == std::vector<std::string>{"kobold_king", "schmiedegolem", "sturmwaechter", "kobold_king"});
}

TEST_CASE("Gleiche Startzahl ergibt gleiche Wellen") {
    std::mt19937 a(42), b(42);
    CHECK(ArenaDirector::composeWave(7, a) == ArenaDirector::composeWave(7, b));
}

TEST_CASE("Ablauf: Wartezeit, Welle, Ende der Welle, Pause, nächste Welle") {
    ArenaDirector d(3, {{-5, 0, 0}, {5, 0, 0}});
    auto u = d.update(kDt, 0);
    CHECK_FALSE(u.waveStarted);  // erst nach der Wartezeit

    ArenaDirector::Update started;
    for (int i = 0; i < 400 && !started.waveStarted; i++) started = d.update(kDt, 0);
    REQUIRE(started.waveStarted);
    CHECK(d.wave() == 1);
    CHECK_FALSE(started.spawns.empty());
    CHECK(d.waveActive());

    // Solange Gegner leben, endet die Welle nicht
    for (int i = 0; i < 120; i++) CHECK_FALSE(d.update(kDt, 2).waveCleared);

    auto cleared = d.update(kDt, 0);
    CHECK(cleared.waveCleared);
    CHECK_FALSE(cleared.offerSegen);  // Welle 1: noch kein Segen
    CHECK_FALSE(d.waveActive());

    ArenaDirector::Update next;
    for (int i = 0; i < 400 && !next.waveStarted; i++) next = d.update(kDt, 0);
    CHECK(next.waveStarted);
    CHECK(d.wave() == 2);
}

TEST_CASE("Alle drei Wellen gibt es einen Segen") {
    ArenaDirector d(1);
    std::vector<bool> offers;
    for (int w = 1; w <= 6; w++) {
        ArenaDirector::Update u;
        for (int i = 0; i < 1000 && !u.waveStarted; i++) u = d.update(kDt, 0);
        REQUIRE(u.waveStarted);
        offers.push_back(d.update(kDt, 0).offerSegen);
    }
    CHECK(offers == std::vector<bool>{false, false, true, false, false, true});
}

TEST_CASE("Jede zehnte Welle ist eine Bosswelle und endet erst, wenn der Boss besiegt ist") {
    ArenaDirector d(1);
    ArenaDirector::Update u;
    for (int w = 1; w <= 10; w++) {
        u = {};
        for (int i = 0; i < 1000 && !u.waveStarted; i++) u = d.update(kDt, 0);
        REQUIRE(u.waveStarted);
        if (w < 10) {
            CHECK_FALSE(u.boss.has_value());
            d.update(kDt, 0);  // Welle beenden
        }
    }
    REQUIRE(u.boss.has_value());
    CHECK(*u.boss == "kobold_king");
    CHECK(u.spawns.empty());
    CHECK_FALSE(d.update(kDt, 0, /*bossAlive=*/true).waveCleared);
    auto end = d.update(kDt, 0, false);
    CHECK(end.waveCleared);
    CHECK(end.offerSegen);
}

TEST_CASE("Spawnpunkte stammen aus der Liste (mit kleiner Streuung)") {
    ArenaDirector d(2, {{10, 0, 10}});
    ArenaDirector::Update u;
    for (int i = 0; i < 400 && !u.waveStarted; i++) u = d.update(kDt, 0);
    for (const SpawnRequest& s : u.spawns) {
        CHECK(std::fabs(s.position.x - 10.0f) < 1.5f);
        CHECK(std::fabs(s.position.z - 10.0f) < 1.5f);
    }
}
