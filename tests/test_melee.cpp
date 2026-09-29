#include "combat/melee.h"
#include "doctest.h"

using namespace aldoria;

namespace {
// Blickt nach +Z (yaw 0), Reichweite 2, Halbwinkel 45 Grad
MeleeQuery forwardQuery() { return {{0, 0, 0}, 0.0f, 2.0f, 0.785f}; }
}  // namespace

TEST_CASE("Ziel direkt vor dem Angreifer wird getroffen") {
    CHECK(meleeHits(forwardQuery(), {0, 0, 1.5f}, 0.4f, 1.0f));
}

TEST_CASE("Ziel hinter dem Angreifer wird nicht getroffen") {
    CHECK_FALSE(meleeHits(forwardQuery(), {0, 0, -1.5f}, 0.4f, 1.0f));
}

TEST_CASE("Zu weit weg ist kein Treffer, die Zielbreite zählt aber zur Reichweite") {
    CHECK_FALSE(meleeHits(forwardQuery(), {0, 0, 3.0f}, 0.4f, 1.0f));
    CHECK(meleeHits(forwardQuery(), {0, 0, 2.3f}, 0.4f, 1.0f));
}

TEST_CASE("Seitlich außerhalb des Kegels trifft nicht, breite Ziele streifen den Rand") {
    CHECK_FALSE(meleeHits(forwardQuery(), {1.6f, 0, 0.2f}, 0.1f, 1.0f));
    // Mittelpunkt knapp außerhalb (ca. 50 Grad), aber das Ziel ist breit
    CHECK(meleeHits(forwardQuery(), {1.2f, 0, 1.0f}, 0.5f, 1.0f));
}

TEST_CASE("Rundum-Angriff trifft in alle Richtungen") {
    MeleeQuery q{{0, 0, 0}, 0.0f, 2.0f, 3.14159265f};
    CHECK(meleeHits(q, {0, 0, -1.5f}, 0.4f, 1.0f));
    CHECK(meleeHits(q, {-1.5f, 0, 0}, 0.4f, 1.0f));
}

TEST_CASE("Höhenunterschied verhindert Treffer") {
    CHECK_FALSE(meleeHits(forwardQuery(), {0, 5.0f, 1.0f}, 0.4f, 1.0f));   // weit oben
    CHECK_FALSE(meleeHits(forwardQuery(), {0, -3.0f, 1.0f}, 0.4f, 1.0f));  // weit unten
}

TEST_CASE("Blickrichtung dreht den Kegel mit") {
    MeleeQuery q = forwardQuery();
    q.facingYaw = 1.5707963f;  // blickt nach +X
    CHECK(meleeHits(q, {1.5f, 0, 0}, 0.4f, 1.0f));
    CHECK_FALSE(meleeHits(q, {0, 0, 1.5f}, 0.4f, 1.0f));
}
