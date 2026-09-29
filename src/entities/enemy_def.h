#pragma once

#include <optional>
#include <string>

#include "core/json.h"
#include "raylib.h"

namespace aldoria {

enum class EnemyBehavior {
    Melee,   // läuft heran, warnt, schlägt zu
    Ranged,  // hält Abstand, zielt (Vorwarnung), schießt
    Leaper   // springt aus der Ferne auf den Spieler
};

// Gegnerwerte aus data/enemies/<id>.json. Zeiten in Sekunden, Winkel in Radiant (JSON: Grad).
struct EnemyDef {
    std::string id = "enemy";
    std::string name = "Gegner";
    EnemyBehavior behavior = EnemyBehavior::Melee;
    float maxHealth = 30.0f;
    float radius = 0.45f;
    float height = 1.1f;

    float chaseSpeed = 3.6f;
    float sightRange = 9.0f;
    float loseRange = 16.0f;

    float attackRange = 1.6f;
    float telegraph = 0.55f;        // Vorwarnung vor dem Angriff: der Spieler hat Zeit zu reagieren
    float attackActive = 0.18f;
    float attackRecovery = 0.8f;    // danach ist der Gegner angreifbar
    float attackLungeSpeed = 7.0f;
    float attackHalfAngle = 1.05f;
    float damage = 8.0f;
    float knockback = 6.0f;

    float stagger = 0.35f;          // Betäubungszeit bei einem Treffer
    bool interruptible = true;      // unterbricht ein Treffer die Vorwarnung/den Angriff?
    Color color{200, 90, 70, 255};
    std::string skin;               // Material der Haut (leer = je nach Art und Name gewählt)
    float spawnTime = 0.9f;         // Dauer des Auftauchens bei Arena-Wellen

    // Fernkämpfer
    float preferredMin = 5.0f;
    float preferredMax = 8.0f;
    float shootCooldown = 2.0f;
    float projectileSpeed = 12.0f;

    // Springer
    float leapRange = 6.5f;
    float leapSpeed = 11.0f;
    float leapHeight = 7.0f;
    float leapCooldown = 2.2f;

    // Beute
    float healthDropChance = 0.12f; // Chance auf ein Herz beim Tod

    static EnemyDef fromJson(const json& j);
    static std::optional<EnemyDef> loadFromFile(const std::string& path);
};

}  // namespace aldoria
