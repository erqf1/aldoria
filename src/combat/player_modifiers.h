#pragma once

#include <string>
#include <vector>

#include "core/json.h"

namespace aldoria {

// Summe aller Upgrades ("Segen"), die der Spieler gewählt hat. Malwerte (Mult) starten bei 1, Summenwerte bei 0.
struct PlayerModifiers {
    float damageMult = 1.0f;
    float attackSpeedMult = 1.0f;
    float moveSpeedMult = 1.0f;
    float jumpMult = 1.0f;
    float rangeMult = 1.0f;
    float knockbackMult = 1.0f;
    float dodgeCooldownMult = 1.0f;
    float sparkDamageMult = 1.0f;
    float sparkCooldownMult = 1.0f;

    float dodgeIFramesAdd = 0.0f;
    float maxHealthAdd = 0.0f;
    float healOnKill = 0.0f;
    float flaskHealAdd = 0.0f;
    float resistance = 0.0f;  // 0..0,9 Anteil weniger Schaden
    float critChance = 0.0f;  // 0..1

    // Wendet einen Effekt aus der JSON-Datei an. Gibt false zurück, wenn der Name unbekannt ist.
    bool apply(const std::string& effect, float value);
};

struct SegenDef {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::pair<std::string, float>> effects;
};

struct SegenLibrary {
    std::vector<SegenDef> all;

    const SegenDef* find(const std::string& id) const;
    // Summiert die Wirkung aller gewählten Segen
    PlayerModifiers combine(const std::vector<std::string>& chosen) const;

    static SegenLibrary fromJson(const json& j);
    static SegenLibrary loadFromFile(const std::string& path);
};

}  // namespace aldoria
