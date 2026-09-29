#pragma once

#include "raylib.h"

namespace aldoria {

// Kegelförmiger Nahkampf-Bereich in der Bodenebene
struct MeleeQuery {
    Vector3 origin{0, 0, 0};
    float facingYaw = 0.0f;  // Blickrichtung, (sin, cos) in der XZ-Ebene
    float range = 2.0f;
    float halfAngle = 1.0f;  // Radiant; ab PI wirkt der Angriff rundum
};

// Trifft der Angriff ein zylindrisches Ziel? Breite Ziele werden auch beim Streifen getroffen.
bool meleeHits(const MeleeQuery& query, Vector3 targetPosition, float targetRadius, float targetHeight);

}  // namespace aldoria
