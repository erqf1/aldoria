#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/input.h"

namespace aldoria {

struct MenuItem {
    std::string label;
    std::function<void()> onSelect;                 // Bestätigen
    std::function<std::string()> value;             // optionaler Wert rechts neben der Beschriftung
    std::function<void(int)> onAdjust;              // Pfeil links/rechts (-1 / +1)
    std::function<bool()> enabled;                  // optional: ausgegraut, wenn false
};

// Einfaches senkrechtes Menü für Titel, Pause und Einstellungen. Tastatur und Controller über Input-Aktionen.
class Menu {
public:
    void setItems(std::vector<MenuItem> items);
    void update(const Input& input);
    // centerX < 0: Fenstermitte
    void draw(const std::string& title, float centerY, float time, float centerX = -1.0f) const;

    // Hinweiszeile unter dem Rahmen (Tastenhilfe), leer = keine
    std::string hint;

    // Wird bei Auswahl (0), Bestätigen (1) und Zurück (2) aufgerufen, z. B. für Klänge
    std::function<void(int)> sound;

    int selected() const { return selected_; }
    void select(int index);
    size_t size() const { return items_.size(); }
    // Für Tests und Skripte: Eintrag ausführen
    void activate();

private:
    bool isEnabled(int index) const;
    void move(int dir);

    std::vector<MenuItem> items_;
    int selected_ = 0;
};

}  // namespace aldoria
