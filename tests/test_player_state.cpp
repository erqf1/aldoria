#include <cmath>

#include "core/events.h"
#include "doctest.h"
#include "entities/player.h"
#include "raymath.h"
#include "world/kinematic_body.h"

using namespace aldoria;

namespace {

constexpr float kDt = 1.0f / 60.0f;

WeaponDef testWeapon() {
    WeaponDef w;
    w.id = "test";
    w.comboWindow = 0.45f;
    AttackDef a;
    a.name = "A";
    a.windup = 0.10f;
    a.active = 0.10f;
    a.recovery = 0.20f;
    a.lunge = 0.0f;
    AttackDef b = a;
    b.name = "B";
    w.attacks = {a, b};
    return w;
}

// Ein Spieler auf ebenem Boden mit Testwaffe. `run` simuliert Frames mit 60 Hz.
struct Rig {
    Level level = Level::makeFallback();
    EventBus events;
    Player player;
    int jumps = 0, deaths = 0;

    Rig() {
        events.subscribe<PlayerJumped>([this](const PlayerJumped&) { jumps++; });
        events.subscribe<PlayerDied>([this](const PlayerDied&) { deaths++; });
        player.configure(PlayerConfig{});
        player.setWeapon(testWeapon());
        player.spawn({0, 0, 0});
        run(0.2f);  // landen und zur Ruhe kommen
    }

    // Die "gedrückt"-Signale gelten nur für den ersten Frame
    void run(float seconds, PlayerInput in = {}, float cameraYaw = 0.0f) {
        int frames = (int)std::round(seconds / kDt);
        for (int i = 0; i < frames; i++) {
            PlayerFrame f;
            f.level = &level;
            f.cameraYaw = cameraYaw;
            player.update(kDt, in, f, events);
            in.jumpPressed = in.attackPressed = in.dodgePressed = false;
        }
    }
};

PlayerInput press(bool jump, bool attack, bool dodge) {
    PlayerInput in;
    in.jumpPressed = in.jumpDown = jump;
    in.attackPressed = attack;
    in.dodgePressed = dodge;
    return in;
}

}  // namespace

TEST_CASE("Der Spieler steht nach dem Start auf dem Boden") {
    Rig r;
    CHECK(r.player.grounded());
    CHECK(r.player.position().y == doctest::Approx(0.0f));
    CHECK(r.player.state() == PlayerState::Normal);
}

TEST_CASE("Springen hebt den Spieler vom Boden") {
    Rig r;
    r.run(kDt, press(true, false, false));
    CHECK(r.jumps == 1);
    CHECK_FALSE(r.player.grounded());
    CHECK(r.player.velocity().y > 0.0f);
}

TEST_CASE("Doppelsprung: ohne Fähigkeit gibt es keinen zweiten Sprung in der Luft") {
    Rig r;
    r.run(kDt, press(true, false, false));
    r.run(0.25f);
    float vy = r.player.velocity().y;
    r.run(kDt, press(true, false, false));
    CHECK(r.jumps == 1);
    CHECK(r.player.velocity().y < vy);  // kein neuer Schub
    CHECK_FALSE(r.player.airJumpAvailable());
}

TEST_CASE("Doppelsprung: mit Fähigkeit ein zweiter Sprung pro Flug, nach der Landung wieder frei") {
    Rig r;
    r.player.setDoubleJumpUnlocked(true);
    int airJumps = 0;
    r.events.subscribe<PlayerAirJumped>([&](const PlayerAirJumped&) { airJumps++; });

    PlayerInput hold;
    hold.jumpDown = true;  // Taste gedrückt halten, damit der Sprung nicht gekürzt wird
    r.run(kDt, press(true, false, false));
    REQUIRE(r.jumps == 1);
    CHECK(r.player.airJumpAvailable());
    r.run(0.3f, hold);
    float vyBefore = r.player.velocity().y;
    r.run(kDt, press(true, false, false));
    CHECK(airJumps == 1);
    CHECK(r.player.velocity().y > vyBefore + 3.0f);
    CHECK_FALSE(r.player.airJumpAvailable());

    // Ein dritter Sprung in der Luft geht nicht
    r.run(0.2f);
    r.run(kDt, press(true, false, false));
    CHECK(airJumps == 1);

    // Nach der Landung ist die Fähigkeit wieder bereit
    r.run(2.0f);
    REQUIRE(r.player.grounded());
    CHECK(r.player.airJumpAvailable());
}

TEST_CASE("Doppelsprung: kurz vor der Landung zählt ein Druck als Bodensprung und verbraucht ihn nicht") {
    Rig r;
    r.player.setDoubleJumpUnlocked(true);
    int airJumps = 0;
    r.events.subscribe<PlayerAirJumped>([&](const PlayerAirJumped&) { airJumps++; });
    r.player.spawn({0, 0.15f, 0});
    r.run(kDt, press(true, false, false));
    r.run(0.4f);
    CHECK(airJumps == 0);
    CHECK(r.jumps == 1);  // der vorgemerkte Sprung wurde bei der Landung ausgeführt
    CHECK(r.player.airJumpAvailable());
}

TEST_CASE("Sprungpuffer: ein kurz vor der Landung gedrückter Sprung wird nachgeholt") {
    Rig r;
    r.player.spawn({0, 0.15f, 0});  // Landung in ca. 0,1 s, der Puffer hält 0,12 s
    r.run(kDt, press(true, false, false));
    r.run(0.3f);
    CHECK(r.jumps == 1);
}

TEST_CASE("Ein zu früh gedrückter Sprung verfällt") {
    Rig r;
    r.player.spawn({0, 1.0f, 0});  // Fall dauert ca. 0,27 s
    r.run(kDt, press(true, false, false));
    r.run(0.6f);
    CHECK(r.jumps == 0);
}

TEST_CASE("Coyote-Time: kurz nach dem Verlassen einer Kante geht der Sprung noch") {
    for (float wait : {0.05f, 0.3f}) {
        Rig r;
        // Hohe Plattform: der Fall dauert lange genug (ca. 0,46 s), damit 0,3 s Wartezeit noch in der Luft liegen
        LevelBox box;
        box.center = {0, 1.5f, 0};
        box.size = {2, 3, 2};
        r.level.boxes.push_back(box);
        r.player.spawn({0, 3.0f, 0});
        r.run(0.2f);
        REQUIRE(r.player.grounded());

        // Nach +X über die Kante laufen (Kamera blickt mit yaw = 90 Grad nach +X)
        PlayerInput walk;
        walk.move = {0, 1};
        for (int i = 0; i < 300 && r.player.grounded(); i++) r.run(kDt, walk, 1.5707963f);
        REQUIRE_FALSE(r.player.grounded());

        r.run(wait);
        r.run(kDt, press(true, false, false));
        if (wait < 0.1f) CHECK(r.jumps == 1);
        else CHECK(r.jumps == 0);
    }
}

TEST_CASE("Sprung kürzen: Taste früh loslassen gibt einen niedrigeren Sprung") {
    auto peak = [](bool holdJump) {
        Rig r;
        PlayerInput in = press(true, false, false);
        float maxY = 0.0f;
        for (int i = 0; i < 90; i++) {
            PlayerFrame f;
            f.level = &r.level;
            in.jumpDown = holdJump || i < 3;
            r.player.update(kDt, in, f, r.events);
            in.jumpPressed = false;
            maxY = std::max(maxY, r.player.position().y);
        }
        return maxY;
    };
    CHECK(peak(true) > peak(false) + 0.3f);
}

TEST_CASE("Angriff: Ausholen, Trefferphase, Nachschwingen") {
    Rig r;
    r.run(kDt, press(false, true, false));
    CHECK(r.player.state() == PlayerState::Attacking);
    CHECK(r.player.swingId() == 1);
    CHECK_FALSE(r.player.attackActive());  // noch im Ausholen

    r.run(0.13f);
    CHECK(r.player.attackActive());

    r.run(0.1f);
    CHECK(r.player.state() == PlayerState::Attacking);
    CHECK_FALSE(r.player.attackActive());  // Nachschwingen

    r.run(0.3f);
    CHECK(r.player.state() == PlayerState::Normal);
}

TEST_CASE("Kombo: ein Klick im Nachschwingen startet die nächste Stufe") {
    Rig r;
    r.run(kDt, press(false, true, false));
    CHECK(r.player.currentAttack().name == "A");
    r.run(0.23f);  // im Nachschwingen
    r.run(kDt, press(false, true, false));
    CHECK(r.player.state() == PlayerState::Attacking);
    CHECK(r.player.currentAttack().name == "B");
    CHECK(r.player.swingId() == 2);
}

TEST_CASE("Kombo läuft nach dem Fenster wieder bei Stufe 1 an") {
    {
        Rig r;
        r.run(kDt, press(false, true, false));
        r.run(0.5f);  // Angriff vorbei
        r.run(0.1f);
        r.run(kDt, press(false, true, false));
        CHECK(r.player.currentAttack().name == "B");
    }
    {
        Rig r;
        r.run(kDt, press(false, true, false));
        r.run(0.5f);
        r.run(0.7f);  // Kombo-Fenster (0,45 s) abgelaufen
        r.run(kDt, press(false, true, false));
        CHECK(r.player.currentAttack().name == "A");
    }
}

TEST_CASE("Ausweichen: kurz unverwundbar, dann wieder verwundbar, bewegt den Spieler") {
    Rig r;
    Vector3 start = r.player.position();
    r.run(kDt, press(false, false, true));
    CHECK(r.player.state() == PlayerState::Dodging);
    CHECK_FALSE(r.player.canBeHit());

    r.run(0.15f);
    CHECK_FALSE(r.player.canBeHit());
    r.run(0.15f);  // nach den Unverwundbarkeits-Frames, aber noch im Ausweichen
    CHECK(r.player.state() == PlayerState::Dodging);
    CHECK(r.player.canBeHit());

    r.run(0.2f);
    CHECK(r.player.state() == PlayerState::Normal);
    float moved = std::hypot(r.player.position().x - start.x, r.player.position().z - start.z);
    CHECK(moved > 2.0f);
}

TEST_CASE("Aus dem Nachschwingen kann man ausweichen") {
    Rig r;
    r.run(kDt, press(false, true, false));
    r.run(0.23f);
    r.run(kDt, press(false, false, true));
    CHECK(r.player.state() == PlayerState::Dodging);
}

TEST_CASE("Treffer: Schaden, Betäubung, danach kurz unverwundbar") {
    Rig r;
    float dealt = r.player.takeDamage(30.0f, {2, 0, 0}, r.events);
    CHECK(dealt == doctest::Approx(30.0f));
    CHECK(r.player.health().current() == doctest::Approx(70.0f));
    CHECK(r.player.state() == PlayerState::Hurt);

    // Direkt danach zählt ein weiterer Treffer nicht
    CHECK(r.player.takeDamage(30.0f, {0, 0, 0}, r.events) == 0.0f);
    CHECK(r.player.health().current() == doctest::Approx(70.0f));

    r.run(0.4f);
    CHECK(r.player.state() == PlayerState::Normal);
    r.run(0.7f);  // Unverwundbarkeit (0,9 s) ist vorbei
    CHECK(r.player.takeDamage(10.0f, {0, 0, 0}, r.events) == doctest::Approx(10.0f));
}

TEST_CASE("Ein Treffer unterbricht den Angriff") {
    Rig r;
    r.run(kDt, press(false, true, false));
    r.player.takeDamage(10.0f, {0, 0, 0}, r.events);
    CHECK(r.player.state() == PlayerState::Hurt);
}

TEST_CASE("Tod: ein Ereignis, danach keine Aktionen mehr, Respawn füllt auf") {
    Rig r;
    r.player.takeDamage(1000.0f, {0, 0, 0}, r.events);
    CHECK(r.player.dead());
    CHECK(r.deaths == 1);
    CHECK(r.player.takeDamage(10.0f, {0, 0, 0}, r.events) == 0.0f);

    r.run(kDt, press(true, true, true));
    CHECK(r.player.dead());
    CHECK(r.jumps == 0);

    r.player.respawn();
    CHECK_FALSE(r.player.dead());
    CHECK(r.player.health().current() == doctest::Approx(100.0f));
}

TEST_CASE("Eingaben während des Hit-Stops (dt = 0) gehen nicht verloren") {
    Rig r;
    PlayerFrame f;
    f.level = &r.level;
    r.player.update(0.0f, press(false, true, false), f, r.events);  // Spielzeit steht
    CHECK(r.player.state() == PlayerState::Normal);
    r.run(kDt);  // erster normaler Frame
    CHECK(r.player.state() == PlayerState::Attacking);
}

TEST_CASE("KinematicBody: fällt, landet und meldet die Aufprallgeschwindigkeit") {
    Level level = Level::makeFallback();
    KinematicBody body;
    body.position = {0, 2.0f, 0};
    float impact = 0.0f;
    for (int i = 0; i < 120 && !body.grounded; i++) impact = std::max(impact, body.step(kDt, level, 28.0f, 40.0f));
    CHECK(body.grounded);
    CHECK(body.position.y == doctest::Approx(0.0f));
    CHECK(impact > 8.0f);
    // Am Boden stehend gibt es keinen neuen Aufprall
    CHECK(body.step(kDt, level, 28.0f, 40.0f) == 0.0f);
}

// ---------------------------------------------------------------- Umhang

TEST_CASE("Umhang: bleibt außerhalb des Körpers, ruckelt nicht und fällt nicht durch den Boden") {
    Rig r;
    r.run(0.5f);
    std::array<Vector3, kClothCols * kClothRows> prev = r.player.capeGrid();
    Vector3 prevPlayer = r.player.position();
    struct Worst {
        float pen = 0.0f, step = 0.0f, low = 1e9f;
    };
    auto check = [&](const char* name, int frames, PlayerInput in, float cameraYaw, float maxPen, float maxStep) {
        Worst w;
        for (int i = 0; i < frames; i++) {
            r.run(kDt, in, cameraYaw);
            in.jumpPressed = in.dodgePressed = in.attackPressed = false;
            const auto& g = r.player.capeGrid();
            const Vector3 shift = Vector3Subtract(r.player.position(), prevPlayer);   // die Figur selbst bewegt sich mit
            prevPlayer = r.player.position();
            for (size_t k = 0; k < g.size(); k++) {
                REQUIRE(std::isfinite(g[k].x + g[k].y + g[k].z));
                w.step = std::max(w.step, Vector3Distance(Vector3Subtract(g[k], prev[k]), shift));
                w.low = std::min(w.low, g[k].y - r.player.position().y);
            }
            w.pen = std::max(w.pen, r.player.capePenetration());
            prev = g;
        }
        INFO(name << ": Durchdringung " << w.pen << " m, größter Schritt " << w.step << " m, tiefster Punkt " << w.low << " m");
        CHECK(w.pen <= maxPen);
        CHECK(w.step <= maxStep);
        CHECK(w.low > -0.05f);   // nie unter die Füße
    };
    PlayerInput run;
    run.move = {0, 1};
    run.sprint = true;
    check("Sprint", 40, run, 0.0f, 0.01f, 0.3f);
    check("Kurve", 30, run, 1.5f, 0.01f, 0.3f);
    check("Kehrtwende", 30, run, -2.4f, 0.01f, 0.3f);
    PlayerInput jump = run;
    jump.jumpPressed = jump.jumpDown = true;
    check("Sprung", 60, jump, 0.0f, 0.01f, 0.3f);
    PlayerInput attack;
    attack.attackPressed = true;
    check("Schwerthieb", 60, attack, 0.0f, 0.01f, 0.3f);
    check("Ruhe", 60, PlayerInput{}, 0.0f, 0.01f, 0.3f);
    PlayerInput dodge = run;
    dodge.dodgePressed = true;
    // Die Rolle dreht den ganzen Körper in Sekundenbruchteilen; der Stoff darf mitgerissen werden, aber nicht im Körper stecken
    check("Ausweichrolle", 50, dodge, 0.3f, 0.06f, 0.45f);
}

TEST_CASE("Umhang: hängt in Ruhe hinter dem Rücken und reicht bis zu den Knien") {
    Rig r;
    r.run(1.5f);
    const auto& g = r.player.capeGrid();
    Vector3 fwd{std::sin(r.player.yaw()), 0, std::cos(r.player.yaw())};
    Vector3 p = r.player.position();
    const Vector3& hem = g[(size_t)((kClothRows - 1) * kClothCols + kClothCols / 2)];
    float back = -Vector3DotProduct(Vector3Subtract(hem, p), fwd);
    CHECK(back > 0.2f);              // hinter dem Körper
    CHECK(back < 0.7f);
    CHECK(hem.y - p.y > 0.2f);       // Saum hängt nicht auf dem Boden
    CHECK(hem.y - p.y < 0.8f);       // ... aber lang genug bis unter die Hüfte
}

TEST_CASE("Umhang: bei hoher und schwankender Bildrate ebenso stabil") {
    Rig r;
    PlayerInput run;
    run.move = {0, 1};
    run.sprint = true;
    float worstPen = 0.0f, worstStep = 0.0f;
    std::array<Vector3, kClothCols * kClothRows> prev = r.player.capeGrid();
    Vector3 prevPlayer = r.player.position();
    const float dts[] = {1.0f / 200.0f, 1.0f / 144.0f, 1.0f / 30.0f, 1.0f / 90.0f};
    for (int i = 0; i < 600; i++) {
        float dt = dts[(i / 7) % 4];
        PlayerFrame f;
        f.level = &r.level;
        f.cameraYaw = (i < 300) ? 0.0f : 2.0f;
        r.player.update(dt, run, f, r.events);
        const auto& g = r.player.capeGrid();
        Vector3 shift = Vector3Subtract(r.player.position(), prevPlayer);
        prevPlayer = r.player.position();
        for (size_t k = 0; k < g.size(); k++) {
            REQUIRE(std::isfinite(g[k].x + g[k].y + g[k].z));
            worstStep = std::max(worstStep, Vector3Distance(Vector3Subtract(g[k], prev[k]), shift) / (dt * 60.0f));
        }
        worstPen = std::max(worstPen, r.player.capePenetration());
        prev = g;
    }
    CHECK(worstPen < 0.02f);
    CHECK(worstStep < 0.4f);   // pro 1/60 s umgerechnet
}
