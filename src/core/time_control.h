#pragma once

#include <algorithm>

namespace aldoria {

// Spielzeit getrennt von der echten Zeit: Hit-Stop friert das Spiel kurz ein, `scale` erlaubt Zeitlupe.
struct TimeControl {
    float scale = 1.0f;
    float hitstopRemaining = 0.0f;

    void hitstop(float seconds) { hitstopRemaining = std::max(hitstopRemaining, seconds); }

    // Rechnet die echte Frame-Zeit in Spielzeit um
    float advance(float realDt) {
        if (hitstopRemaining > 0.0f) {
            hitstopRemaining = std::max(0.0f, hitstopRemaining - realDt);
            return 0.0f;
        }
        return realDt * scale;
    }
};

}  // namespace aldoria
