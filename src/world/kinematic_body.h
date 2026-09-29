#pragma once

#include "raylib.h"
#include "world/level.h"

namespace aldoria {

// Gemeinsame Bewegungsphysik für Spieler und Gegner: Schwerkraft, Wände, Stufen, Boden.
// Die Steuerung setzt `velocity`; die Kollision übernimmt step().
struct KinematicBody {
    Vector3 position{0, 0, 0};  // Fußpunkt
    Vector3 velocity{0, 0, 0};
    float radius = 0.4f;
    float height = 1.8f;
    bool grounded = false;

    // Gibt die Aufprallgeschwindigkeit zurück, wenn der Körper in diesem Schritt gelandet ist, sonst 0.
    float step(float dt, const Level& level, float gravity, float maxFallSpeed);
};

}  // namespace aldoria
