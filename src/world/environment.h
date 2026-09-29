#pragma once

#include "raylib.h"

namespace aldoria {

// Licht- und Nebelstimmung einer Welt. Farben sind Werte von 0 bis 1.
struct Environment {
    Vector3 sunDirection{-0.45f, -0.8f, -0.35f};  // Richtung, in die das Licht scheint
    Vector3 sunColor{1.0f, 0.93f, 0.78f};
    Vector3 skyAmbient{0.42f, 0.52f, 0.68f};      // Streulicht von oben
    Vector3 groundAmbient{0.24f, 0.21f, 0.17f};   // Streulicht von unten
    Vector3 fogColor{0.64f, 0.74f, 0.84f};
    Vector3 skyTopColor{0.36f, 0.55f, 0.85f};
    float fogStart = 40.0f;
    float fogEnd = 140.0f;

    // Bildstimmung (Nachbearbeitung)
    float exposure = 1.0f;       // Gesamthelligkeit
    float bloom = 0.55f;         // Leuchten heller Flächen
    float saturation = 1.08f;
    float vignette = 0.35f;      // Abdunklung an den Bildrändern
    float cloudAmount = 0.35f;   // 0 = wolkenlos, 1 = bedeckt
    float sunGlow = 1.0f;        // Helligkeit von Sonnenscheibe und Hof
    float ambientBoost = 1.0f;   // Faktor auf das Himmelslicht
    float sunBoost = 1.0f;       // Faktor auf das Sonnenlicht
    Vector3 grade{1.0f, 1.0f, 1.0f};  // Farbfilter (Wärme)
};

}  // namespace aldoria
