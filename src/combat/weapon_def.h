#pragma once

#include <string>
#include <vector>

#include "core/json.h"

namespace aldoria {

// Ein Schlag einer Kombo. Alle Zeiten in Sekunden, Winkel in Radiant (in der JSON-Datei in Grad).
struct AttackDef {
    std::string name = "Hieb";
    float damage = 10.0f;
    float windup = 0.10f;    // Ausholen: noch kein Schaden, Gegner kann reagieren
    float active = 0.12f;    // Trefferphase
    float recovery = 0.18f;  // Nachschwingen: hier kann die nächste Kombo-Stufe oder ein Ausweichen einsetzen
    float range = 2.2f;
    float halfAngle = 0.95f;
    float lunge = 1.0f;      // Vorwärtsbewegung während der Trefferphase (Meter)
    float knockback = 4.0f;
    float hitstop = 0.05f;   // kurzes Einfrieren bei Treffern
    float sweepFrom = -1.2f; // Schwertbogen relativ zur Blickrichtung (nur Optik)
    float sweepTo = 1.2f;
    float moveScale = 0.3f;  // Bewegungstempo während des Ausholens

    float total() const { return windup + active + recovery; }
};

struct WeaponDef {
    std::string id = "unarmed";
    std::string name = "Unbewaffnet";
    float comboWindow = 0.45f;  // so lange nach einem Schlag kann die Kombo weitergehen
    std::vector<AttackDef> attacks;

    bool valid() const { return !attacks.empty(); }

    static WeaponDef fromJson(const json& j);
    // Bei Fehlern kommt eine ungültige Waffe (attacks leer) zurück und der Grund steht im Log.
    static WeaponDef loadFromFile(const std::string& path);
};

}  // namespace aldoria
