#pragma once

#include <string>

#include "raylib.h"
#include "world/level.h"

namespace aldoria {

struct DebugInfo {
    std::string levelName;
    std::string playerState;
    Vector3 playerPosition{0, 0, 0};
    Vector3 playerVelocity{0, 0, 0};
    bool grounded = false;
    bool flyCamera = false;
    bool mouseCaptured = true;
    bool litShader = true;
    size_t boxCount = 0;
    size_t enemyCount = 0;
    int comboStep = 0;
};

// Entwicklerwerkzeug: FPS, Koordinaten, Log, Kollisionskörper. Nicht Teil der normalen Spieler-UI.
class DebugOverlay {
public:
    bool visible = true;
    bool showCollision = false;

    void draw(const DebugInfo& info) const;
    void drawCollision(const Level& level) const;
};

}  // namespace aldoria
