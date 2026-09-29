#pragma once

#include <functional>
#include <vector>

#include "core/input.h"
#include "core/settings.h"
#include "ui/menu.h"

namespace aldoria {

// Einstellungen mit Unterseiten: Hauptseite (Grafik, Ton, Maus), "Tasten belegen" (alle Aktionen) und pro Aktion
// eine Seite mit ihren Tasten (mehrere möglich, ersetzen, entfernen, auf Standard).
// `onClose` wird aufgerufen, wenn die Hauptseite verlassen wird, `onChange` nach jeder Änderung (zum Speichern).
class SettingsScreen {
public:
    SettingsScreen(Settings& settings, Input& input, std::function<void()> onClose, std::function<void()> onChange);

    // Öffnet die Hauptseite
    void open();
    void update(const Input& input);
    void draw(float centerY, float time, float centerX = -1.0f) const;

    void setSound(std::function<void(int)> sound);

    enum class Page { Main, Keys, Action };
    Page page() const { return page_; }
    // Für Tests: ist die Aufnahme einer Taste gerade offen, und wie viele Zeilen hat die aktuelle Seite?
    size_t rows() const { return current().size(); }
    Menu& current() { return page_ == Page::Main ? main_ : (page_ == Page::Keys ? keys_ : action_); }
    const Menu& current() const { return page_ == Page::Main ? main_ : (page_ == Page::Keys ? keys_ : action_); }

private:
    void buildMain();
    void buildKeys();
    void buildAction();
    void goBack();

    Settings& settings_;
    Input& input_;
    std::function<void()> onClose_, onChange_;
    Menu main_, keys_, action_;
    Page page_ = Page::Main;
    Action editing_ = Action::Jump;
    int seenRevision_ = 0;
};

}  // namespace aldoria
