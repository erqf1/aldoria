#include "core/math_util.h"
#include "core/time_control.h"
#include "doctest.h"

using namespace aldoria;

TEST_CASE("Hit-Stop friert die Spielzeit ein und läuft in echter Zeit ab") {
    TimeControl t;
    CHECK(t.advance(0.016f) == doctest::Approx(0.016f));

    t.hitstop(0.05f);
    CHECK(t.advance(0.02f) == 0.0f);
    CHECK(t.advance(0.02f) == 0.0f);
    CHECK(t.advance(0.02f) == 0.0f);          // Rest von 0.01 läuft ab
    CHECK(t.advance(0.02f) == doctest::Approx(0.02f));
}

TEST_CASE("Kürzerer Hit-Stop verkürzt einen längeren nicht") {
    TimeControl t;
    t.hitstop(0.1f);
    t.hitstop(0.02f);
    CHECK(t.hitstopRemaining == doctest::Approx(0.1f));
}

TEST_CASE("Zeitlupe skaliert die Spielzeit") {
    TimeControl t;
    t.scale = 0.5f;
    CHECK(t.advance(0.02f) == doctest::Approx(0.01f));
}

TEST_CASE("approach bewegt in Schritten und überschießt nicht") {
    CHECK(approach(0.0f, 10.0f, 3.0f) == doctest::Approx(3.0f));
    CHECK(approach(9.0f, 10.0f, 3.0f) == doctest::Approx(10.0f));
    CHECK(approach(5.0f, 0.0f, 2.0f) == doctest::Approx(3.0f));
}

TEST_CASE("turnToward nimmt den kürzesten Weg über die Winkelgrenze") {
    // von knapp unter +PI nach knapp über -PI: kurzer Weg führt über die Grenze
    float r = turnToward(3.0f, -3.0f, 0.1f);
    CHECK(r > 3.0f);
    CHECK(turnToward(0.0f, 0.05f, 0.1f) == doctest::Approx(0.05f));
}

TEST_CASE("Farbe aus Gleitkommawerten wird begrenzt") {
    Color c = toColor({2.0f, -1.0f, 0.5f});
    CHECK(c.r == 255);
    CHECK(c.g == 0);
    CHECK(c.b == 127);
}
