#include "core/build_config.h"
#include "doctest.h"
#include "entities/player.h"

using namespace aldoria;

TEST_CASE("Spielerwerte kommen aus JSON, fehlende Felder behalten die Vorgabe") {
    PlayerConfig defaults;
    PlayerConfig c = PlayerConfig::fromJson(json::parse(R"({ "walk_speed": 3.0, "jump_speed": 12 })"));
    CHECK(c.walkSpeed == doctest::Approx(3.0f));
    CHECK(c.jumpSpeed == doctest::Approx(12.0f));
    CHECK(c.gravity == doctest::Approx(defaults.gravity));
}

TEST_CASE("Kein Objekt: Vorgaben statt Absturz") {
    PlayerConfig c = PlayerConfig::fromJson(json::parse("[1,2,3]"));
    CHECK(c.walkSpeed == doctest::Approx(PlayerConfig{}.walkSpeed));
}

TEST_CASE("Fehlende Datei liefert Vorgaben") {
    PlayerConfig c = PlayerConfig::loadFromFile("gibt/es/nicht.json");
    CHECK(c.radius == doctest::Approx(PlayerConfig{}.radius));
}

TEST_CASE("Die mitgelieferte player.json ist gültig") {
    PlayerConfig c = PlayerConfig::loadFromFile(std::string(ALDORIA_DEV_DATA_DIR) + "/config/player.json");
    CHECK(c.jumpSpeed > 0.0f);
    CHECK(c.height > c.radius * 2.0f);
    CHECK(c.maxHealth > 0.0f);
    CHECK(c.dodgeIFrames < c.dodgeDuration);
}

TEST_CASE("Kampf- und Sprungwerte werden aus JSON gelesen") {
    PlayerConfig c = PlayerConfig::fromJson(json::parse(R"({ "coyote_time": 0.2, "dodge_iframes": 0.1, "max_health": 250 })"));
    CHECK(c.coyoteTime == doctest::Approx(0.2f));
    CHECK(c.dodgeIFrames == doctest::Approx(0.1f));
    CHECK(c.maxHealth == doctest::Approx(250.0f));
    CHECK(c.dodgeSpeed == doctest::Approx(PlayerConfig{}.dodgeSpeed));
}
