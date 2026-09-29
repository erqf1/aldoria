#include "core/input.h"
#include "core/settings.h"
#include "doctest.h"
#include "ui/settings_menu.h"

using namespace aldoria;

TEST_CASE("Tasten: Standardbelegung hat Namen") {
    Input in;
    CHECK(in.bindingName(Action::Jump) == "Leertaste");
    CHECK(in.bindingName(Action::Interact) == "E");
    CHECK(in.bindingName(Action::Attack) == "Maus links");
    CHECK(in.bindingName(Action::Dodge) == "Strg links");
    CHECK(in.bindingNames(Action::MoveForward) == "W, Pfeil hoch");
    CHECK(in.exportBindings().empty());
}

TEST_CASE("Tasten: mehrere Tasten pro Aktion, Controller bleibt") {
    Input in;
    in.setUserBindings(Action::Jump, {{Binding::Device::Key, KEY_F}, {Binding::Device::Key, KEY_G}, {Binding::Device::Mouse, MOUSE_BUTTON_SIDE}});
    CHECK(in.bindingNames(Action::Jump) == "F, G, Maus 4");
    CHECK(in.userBindings(Action::Jump).size() == 3);
    CHECK(in.exportBindings().at("jump") == "key:70,key:71,mouse:3");
    // Höchstens vier, Doppelte fallen weg
    in.setUserBindings(Action::Jump, {{Binding::Device::Key, KEY_F}, {Binding::Device::Key, KEY_F}, {Binding::Device::Key, KEY_A}, {Binding::Device::Key, KEY_B},
                                      {Binding::Device::Key, KEY_C}, {Binding::Device::Key, KEY_D}});
    CHECK(in.userBindings(Action::Jump).size() == Input::kMaxUserBindings);
    CHECK(in.bindingNames(Action::Jump) == "F, A, B, C");
}

TEST_CASE("Tasten: dieselbe Taste darf mehreren Aktionen gehören") {
    Input in;
    in.setUserBindings(Action::Heal, {{Binding::Device::Key, KEY_E}});
    CHECK(in.bindingName(Action::Heal) == "E");
    CHECK(in.bindingName(Action::Interact) == "E");   // nichts wird getauscht oder geraubt
    auto shared = in.sharedWith(Action::Heal);
    REQUIRE(shared.size() == 1);
    CHECK(shared[0] == "Benutzen");
    CHECK(in.sharedWith(Action::Jump).empty());
}

TEST_CASE("Tasten: speichern, laden, einzeln und komplett auf Standard") {
    Input in;
    int calls = 0;
    in.onRebound = [&calls]() { calls++; };
    in.setUserBindings(Action::Jump, {{Binding::Device::Key, KEY_F}});
    in.setUserBindings(Action::Attack, {});
    CHECK(calls == 2);
    auto saved = in.exportBindings();
    CHECK(saved.at("jump") == "key:70");
    CHECK(saved.at("attack") == "none");
    CHECK(in.bindingName(Action::Attack) != "Maus links");

    Input other;
    other.importBindings(saved);
    CHECK(other.bindingName(Action::Jump) == "F");
    CHECK(other.userBindings(Action::Attack).empty());

    in.resetAction(Action::Jump);
    CHECK(in.bindingName(Action::Jump) == "Leertaste");
    CHECK(in.isDefault(Action::Jump));
    CHECK_FALSE(in.isDefault(Action::Attack));
    in.resetAllBindings();
    CHECK(in.exportBindings().empty());
    CHECK(in.bindingName(Action::Attack) == "Maus links");
    CHECK(calls == 4);
}

TEST_CASE("Tasten: wer die Standardtasten wieder setzt, hat keine Abweichung gespeichert") {
    Input in;
    in.setUserBindings(Action::Interact, {{Binding::Device::Key, KEY_X}});
    CHECK_FALSE(in.exportBindings().empty());
    in.setUserBindings(Action::Interact, {{Binding::Device::Key, KEY_E}});
    CHECK(in.exportBindings().empty());
}

TEST_CASE("Tasten: einzelne Taste entfernen") {
    Input in;
    in.removeUserBinding(Action::MoveForward, 0);
    CHECK(in.bindingNames(Action::MoveForward) == "Pfeil hoch");
    in.removeUserBinding(Action::MoveForward, 5);   // ungültiger Platz: nichts passiert
    CHECK(in.bindingNames(Action::MoveForward) == "Pfeil hoch");
}

TEST_CASE("Tasten: Unsinn in der Datei wird ignoriert, Menütasten lassen sich nicht ändern") {
    Input in;
    in.importBindings({{"jump", "quatsch"}, {"gibtsnicht", "key:1"}, {"up", "key:70"}, {"confirm", "key:71"}, {"heal", "key:x"}, {"dodge", "key:70,kaputt"}});
    CHECK(in.bindingName(Action::Jump) == "Leertaste");
    CHECK(in.bindingName(Action::UiUp) == "W");
    CHECK(in.bindingName(Action::Heal) == "R");
    CHECK(in.bindingName(Action::Dodge) == "Strg links");
    CHECK(in.exportBindings().empty());
}

TEST_CASE("Tasten: jede änderbare Aktion hat einen Namen und ist bekannt") {
    Input in;
    for (const RebindableAction& r : rebindableActions()) {
        CHECK(std::string(actionName(r.action)) != "");
        Action back;
        REQUIRE(actionFromName(actionName(r.action), back));
        CHECK(back == r.action);
        CHECK(in.bindingName(r.action) != "-");
    }
}

TEST_CASE("Einstellungen: Unterseiten für die Tasten") {
    Settings settings;
    Input in;
    bool closed = false;
    SettingsScreen screen(settings, in, [&closed]() { closed = true; }, []() {});
    CHECK(screen.page() == SettingsScreen::Page::Main);

    // Hauptseite: "Tasten belegen" ist der vorletzte Eintrag
    screen.current().select((int)screen.rows() - 2);
    screen.current().activate();
    CHECK(screen.page() == SettingsScreen::Page::Keys);
    CHECK(screen.rows() == rebindableActions().size() + 2);

    // Erste Aktion (Vorwärts): zwei Tasten + "Weitere Taste" + "Standard" + "Zurück"
    screen.current().select(0);
    screen.current().activate();
    CHECK(screen.page() == SettingsScreen::Page::Action);
    CHECK(screen.rows() == 5);

    // Eine Taste kommt dazu: die Seite baut sich beim nächsten Update neu auf
    in.setUserBindings(Action::MoveForward, {{Binding::Device::Key, KEY_W}, {Binding::Device::Key, KEY_UP}, {Binding::Device::Key, KEY_I}});
    screen.update(in);
    CHECK(screen.rows() == 6);

    // Zurück führt Seite für Seite bis zum Schließen
    screen.current().select((int)screen.rows() - 1);
    screen.current().activate();
    CHECK(screen.page() == SettingsScreen::Page::Keys);
    screen.current().select((int)screen.rows() - 1);
    screen.current().activate();
    CHECK(screen.page() == SettingsScreen::Page::Main);
    CHECK_FALSE(closed);
    screen.current().select((int)screen.rows() - 1);
    screen.current().activate();
    CHECK(closed);
}
