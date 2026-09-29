#include "core/autopilot.h"
#include "doctest.h"

using namespace aldoria;

namespace {
constexpr float kDt = 1.0f / 60.0f;
}

TEST_CASE("Autopilot liest ein Skript mit Kommentaren und Leerzeilen") {
    Autopilot a;
    std::string error;
    REQUIRE(a.loadFromString("# Test\n\nwait 1\nhold forward+sprint 2  # laufen\npress attack\nshot bild\nquit\n", error));
    CHECK(a.stepCount() == 5);
    CHECK_FALSE(a.finished());
}

TEST_CASE("Autopilot meldet Fehler mit Zeilennummer") {
    Autopilot a;
    std::string error;
    CHECK_FALSE(a.loadFromString("wait 1\nhold fliegen 2\n", error));
    CHECK(error.find("Zeile 2") != std::string::npos);
    CHECK_FALSE(a.loadFromString("wait abc\n", error));
    CHECK_FALSE(a.loadFromString("hold forward\n", error));
    CHECK_FALSE(a.loadFromString("look 1\n", error));
}

TEST_CASE("Autopilot führt Schritte der Reihe nach aus und liefert Befehle") {
    Autopilot a;
    Input input;
    std::string error;
    REQUIRE(a.loadFromString("wait 0.05\nshot eins\nteleport 1 2 3\nquit\n", error));

    std::vector<AutopilotCommand> commands;
    for (int i = 0; i < 3; i++) a.update(kDt, input, commands);  // 0,05 s Warten
    CHECK(commands.empty());
    for (int i = 0; i < 2; i++) a.update(kDt, input, commands);
    REQUIRE(commands.size() >= 1);
    CHECK(commands[0].name == "shot");
    CHECK(commands[0].args[0] == "eins");
    for (int i = 0; i < 5; i++) a.update(kDt, input, commands);
    REQUIRE(commands.size() == 3);
    CHECK(commands[1].name == "teleport");
    CHECK(commands[1].args.size() == 3);
    CHECK(commands[2].name == "quit");
    CHECK(a.finished());
}

TEST_CASE("Gehaltene Aktionen sind nur für ihre Dauer eingespeist") {
    Autopilot a;
    Input input;
    input.setInjectionMode(true);
    std::string error;
    REQUIRE(a.loadFromString("hold forward 0.05\nwait 0.1\n", error));

    std::vector<AutopilotCommand> commands;
    a.update(kDt, input, commands);
    input.update();
    CHECK(input.down(Action::MoveForward));
    for (int i = 0; i < 4; i++) {
        a.update(kDt, input, commands);
        input.update();
    }
    CHECK_FALSE(input.down(Action::MoveForward));
}

TEST_CASE("press wirkt genau einen Frame lang") {
    Autopilot a;
    Input input;
    input.setInjectionMode(true);
    std::string error;
    REQUIRE(a.loadFromString("press attack\nwait 0.1\n", error));

    std::vector<AutopilotCommand> commands;
    a.update(kDt, input, commands);
    input.update();
    CHECK(input.pressed(Action::Attack));
    a.update(kDt, input, commands);
    input.update();
    CHECK_FALSE(input.down(Action::Attack));
}

TEST_CASE("Aktionsnamen werden erkannt") {
    Action a;
    CHECK(actionFromName("attack", a));
    CHECK(a == Action::Attack);
    CHECK(actionFromName("dodge", a));
    CHECK_FALSE(actionFromName("gibtsnicht", a));
}
