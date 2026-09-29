#include <cmath>

#include "doctest.h"
#include "entities/enemy.h"

using namespace aldoria;

namespace {

constexpr float kDt = 1.0f / 60.0f;

EnemyDef testDef() {
    EnemyDef d;
    d.id = "test_imp";
    d.maxHealth = 30.0f;
    d.sightRange = 8.0f;
    d.loseRange = 14.0f;
    d.attackRange = 1.5f;
    d.chaseSpeed = 4.0f;
    d.telegraph = 0.3f;
    d.attackActive = 0.1f;
    d.attackRecovery = 0.4f;
    d.stagger = 0.3f;
    return d;
}

struct Rig {
    EnemyDef def = testDef();
    Level level = Level::makeFallback();
    Enemy enemy;
    Vector3 playerPos{0, 0, 6};

    Rig() : enemy(&def, {0, 0, 0}) {}

    void run(float seconds, bool playerAlive = true) {
        int frames = (int)std::round(seconds / kDt);
        for (int i = 0; i < frames; i++) enemy.update(kDt, level, playerPos, playerAlive);
    }
    // Läuft bis zum Zustand oder höchstens `maxSeconds`
    bool runUntil(EnemyState s, float maxSeconds) {
        for (int i = 0; i < (int)(maxSeconds / kDt); i++) {
            if (enemy.state() == s) return true;
            enemy.update(kDt, level, playerPos, true);
        }
        return enemy.state() == s;
    }
};

}  // namespace

TEST_CASE("Ein weit entfernter Spieler weckt den Gegner nicht") {
    Rig r;
    r.playerPos = {0, 0, 20};
    r.run(1.0f);
    CHECK(r.enemy.state() == EnemyState::Idle);
}

TEST_CASE("Im Sichtbereich verfolgt der Gegner den Spieler") {
    Rig r;
    r.run(0.1f);
    CHECK(r.enemy.state() == EnemyState::Chase);
    float before = r.enemy.position().z;
    r.run(0.3f);
    CHECK(r.enemy.position().z > before + 0.5f);
}

TEST_CASE("Der Gegner verliert den Spieler außerhalb der Verfolgungsgrenze") {
    Rig r;
    r.run(0.1f);
    REQUIRE(r.enemy.state() == EnemyState::Chase);
    r.playerPos = {0, 0, 50};
    r.run(0.1f);
    CHECK(r.enemy.state() == EnemyState::Idle);
}

TEST_CASE("Angriff mit Vorwarnung: Telegraph, dann Trefferphase, dann Erholung") {
    Rig r;
    REQUIRE(r.runUntil(EnemyState::Telegraph, 4.0f));
    CHECK_FALSE(r.enemy.attackHitActive());  // in der Vorwarnung noch harmlos

    // Die Vorwarnung dauert die eingestellte Zeit
    float telegraphTime = 0.0f;
    while (r.enemy.state() == EnemyState::Telegraph && telegraphTime < 2.0f) {
        r.enemy.update(kDt, r.level, r.playerPos, true);
        telegraphTime += kDt;
    }
    CHECK(telegraphTime == doctest::Approx(0.3f).epsilon(0.15));

    REQUIRE(r.enemy.state() == EnemyState::Attack);
    CHECK(r.enemy.attackHitActive());
    r.enemy.consumeAttack();
    CHECK_FALSE(r.enemy.attackHitActive());  // ein Angriff trifft nur einmal

    REQUIRE(r.runUntil(EnemyState::Recover, 1.0f));
    CHECK_FALSE(r.enemy.attackHitActive());
}

TEST_CASE("Treffer: Schaden, Betäubung und kein Dauer-Stun") {
    Rig r;
    r.playerPos = {0, 0, 20};
    float dealt = r.enemy.takeHit(10.0f, {3, 0, 0});
    CHECK(dealt == doctest::Approx(10.0f));
    CHECK(r.enemy.state() == EnemyState::Hurt);
    CHECK(r.enemy.health().current() == doctest::Approx(20.0f));

    // Die Betäubung endet nach der eingestellten Zeit
    r.run(0.4f);
    CHECK(r.enemy.state() != EnemyState::Hurt);

    // Direkt nach einer Betäubung ist der Gegner kurz immun gegen erneutes Betäuben ...
    r.enemy.takeHit(1.0f, {0, 0, 0});
    CHECK(r.enemy.state() != EnemyState::Hurt);

    // ... aber nicht dauerhaft
    r.run(0.6f);
    r.enemy.takeHit(1.0f, {0, 0, 0});
    CHECK(r.enemy.state() == EnemyState::Hurt);
}

TEST_CASE("Ein Treffer unterbricht die Vorwarnung nur bei unterbrechbaren Gegnern") {
    {
        Rig r;
        REQUIRE(r.runUntil(EnemyState::Telegraph, 4.0f));
        r.enemy.takeHit(5.0f, {0, 0, 0});
        CHECK(r.enemy.state() == EnemyState::Hurt);
    }
    {
        Rig r;
        r.def.interruptible = false;
        REQUIRE(r.runUntil(EnemyState::Telegraph, 4.0f));
        r.enemy.takeHit(5.0f, {0, 0, 0});
        CHECK(r.enemy.state() == EnemyState::Telegraph);
    }
}

TEST_CASE("Tod: Zustand Dead, kein weiterer Schaden, danach entfernbar") {
    Rig r;
    r.playerPos = {0, 0, 20};
    r.enemy.takeHit(1000.0f, {0, 0, 0});
    CHECK(r.enemy.state() == EnemyState::Dead);
    CHECK_FALSE(r.enemy.alive());
    CHECK(r.enemy.takeHit(5.0f, {0, 0, 0}) == 0.0f);
    CHECK_FALSE(r.enemy.removable());
    r.run(1.0f);
    CHECK(r.enemy.removable());
}

TEST_CASE("Ein getroffener wartender Gegner wird aufmerksam") {
    Rig r;
    r.playerPos = {0, 0, 10};  // außerhalb der Sichtweite (8), aber innerhalb der Verfolgungsgrenze (14)
    r.run(0.2f);
    REQUIRE(r.enemy.state() == EnemyState::Idle);
    r.enemy.takeHit(1.0f, {0, 0, 0});
    r.run(0.4f);
    CHECK(r.enemy.state() == EnemyState::Chase);
}

TEST_CASE("Gegner bleiben auf dem Boden") {
    Rig r;
    r.playerPos = {0, 0, 20};
    r.run(0.5f);
    CHECK(r.enemy.body().grounded);
    CHECK(r.enemy.position().y == doctest::Approx(0.0f));
}

namespace {

// Zwei Inseln im Nichts: Insel A bei z in [-3, 3], Insel B bei z in [8, 14]; dazwischen klafft eine Lücke
Level makeIslands() {
    Level lv;
    lv.hasGround = false;
    lv.groundY = -10000.0f;
    lv.killY = -14.0f;
    lv.groundSize = {60, 60};
    LevelBox a;
    a.center = {0, -0.5f, 0};
    a.size = {8, 1, 6};
    LevelBox b;
    b.center = {0, -0.5f, 11};
    b.size = {8, 1, 6};
    lv.boxes.push_back(a);
    lv.boxes.push_back(b);
    return lv;
}

}  // namespace

TEST_CASE("Ein Nahkämpfer läuft nicht über die Kante, auch wenn der Spieler auf der anderen Insel steht") {
    EnemyDef def = testDef();
    def.sightRange = 30.0f;
    def.loseRange = 40.0f;
    Level lv = makeIslands();
    Enemy e(&def, {0, 0, 0});
    for (int i = 0; i < 60 * 8; i++) {
        e.update(kDt, lv, {0, 0, 11}, true);
        REQUIRE(e.position().y > -1.0f);   // nie ins Leere gefallen
    }
    CHECK(e.position().z < 3.4f);   // bleibt an der Kante stehen
    CHECK(e.position().z > 1.5f);   // ist aber bis dorthin gelaufen
}

TEST_CASE("Ein Springer springt nicht ins Leere") {
    EnemyDef def = testDef();
    def.behavior = EnemyBehavior::Leaper;
    def.sightRange = 30.0f;
    def.loseRange = 40.0f;
    def.leapRange = 9.0f;
    def.leapSpeed = 11.0f;
    def.leapHeight = 7.0f;
    def.leapCooldown = 0.6f;
    def.telegraph = 0.3f;
    Level lv = makeIslands();
    Enemy e(&def, {0, 0, 0});
    // Der Spieler steht so, dass der Landepunkt im Abgrund läge (weder auf A noch auf B)
    for (int i = 0; i < 60 * 12; i++) {
        e.update(kDt, lv, {0, 0, 7}, true);
        REQUIRE(e.position().y > -1.0f);
    }
}

TEST_CASE("Ein Springer trägt den Sprung, wenn der Landeplatz fester Boden ist") {
    EnemyDef def = testDef();
    def.behavior = EnemyBehavior::Leaper;
    def.sightRange = 30.0f;
    def.loseRange = 40.0f;
    def.leapRange = 12.0f;
    def.leapSpeed = 11.0f;
    def.leapHeight = 7.0f;
    def.telegraph = 0.3f;
    Level lv;
    lv.hasGround = true;
    lv.groundY = 0.0f;
    Enemy e(&def, {0, 0, 0});
    bool leapt = false;
    for (int i = 0; i < 60 * 6 && !leapt; i++) {
        e.update(kDt, lv, {0, 0, 8}, true);
        leapt = e.state() == EnemyState::Attack;
    }
    CHECK(leapt);
}

TEST_CASE("Ein gefallener Gegner kehrt zum sicheren Punkt zurück") {
    EnemyDef def = testDef();
    Level lv = makeIslands();
    Enemy e(&def, {0, 0, 0});
    for (int i = 0; i < 30; i++) e.update(kDt, lv, {0, 0, 40}, true);
    // Gewaltsam über die Kante schieben (wie ein Stoß oder eine einstürzende Plattform)
    e.body().position = {0, -0.4f, 20};
    e.body().grounded = false;
    for (int i = 0; i < 60 * 3; i++) e.update(kDt, lv, {0, 0, 40}, true);
    CHECK(e.position().y > -5.0f);
    CHECK(e.position().z < 4.0f);
}
