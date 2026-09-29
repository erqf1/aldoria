#include "components/health.h"
#include "doctest.h"

using namespace aldoria;

TEST_CASE("Schaden senkt die Lebenspunkte und meldet den echten Wert") {
    Health h(100.0f);
    CHECK(h.damage(30.0f) == doctest::Approx(30.0f));
    CHECK(h.current() == doctest::Approx(70.0f));
    CHECK(h.fraction() == doctest::Approx(0.7f));
}

TEST_CASE("Schaden geht nie unter null und Tote nehmen keinen Schaden mehr") {
    Health h(20.0f);
    CHECK(h.damage(500.0f) == doctest::Approx(20.0f));
    CHECK(h.dead());
    CHECK(h.damage(10.0f) == 0.0f);
}

TEST_CASE("Unverwundbarkeit blockt Schaden und läuft ab") {
    Health h(100.0f);
    h.grantInvulnerability(0.5f);
    CHECK(h.damage(10.0f) == 0.0f);
    h.update(0.3f);
    CHECK(h.invulnerable());
    h.update(0.3f);
    CHECK_FALSE(h.invulnerable());
    CHECK(h.damage(10.0f) == doctest::Approx(10.0f));
}

TEST_CASE("Heilen ist durch das Maximum begrenzt") {
    Health h(100.0f);
    h.damage(30.0f);
    CHECK(h.heal(50.0f) == doctest::Approx(30.0f));
    CHECK(h.current() == doctest::Approx(100.0f));
    CHECK(h.heal(5.0f) == 0.0f);
}

TEST_CASE("Widerstand verringert den Schaden") {
    Health h(100.0f);
    h.resistance = 0.25f;
    CHECK(h.damage(40.0f) == doctest::Approx(30.0f));
}

TEST_CASE("Maximum ändern: mit und ohne Auffüllen") {
    Health h(100.0f);
    h.damage(60.0f);
    h.setMax(150.0f, false);
    CHECK(h.current() == doctest::Approx(40.0f));
    h.setMax(30.0f, false);
    CHECK(h.current() == doctest::Approx(30.0f));
    h.setMax(200.0f, true);
    CHECK(h.current() == doctest::Approx(200.0f));
}
