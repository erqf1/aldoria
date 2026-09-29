#include "debug/debug_overlay.h"

#include <cmath>

#include "core/log.h"

namespace aldoria {

void DebugOverlay::draw(const DebugInfo& info) const {
    if (!visible) return;

    const Color text = WHITE;
    const Color dim{200, 205, 220, 255};
    const int x = 12;
    int y = 10;
    auto line = [&](const std::string& s, Color c) {
        DrawText(s.c_str(), x + 1, y + 1, 20, Color{0, 0, 0, 200});
        DrawText(s.c_str(), x, y, 20, c);
        y += 24;
    };

    float horizontalSpeed = std::sqrt(info.playerVelocity.x * info.playerVelocity.x + info.playerVelocity.z * info.playerVelocity.z);
    line(TextFormat("%d FPS   %s", GetFPS(), info.levelName.c_str()), text);
    line(TextFormat("Pos %.1f  %.1f  %.1f   Tempo %.1f%s", info.playerPosition.x, info.playerPosition.y, info.playerPosition.z,
                    horizontalSpeed, info.grounded ? "" : "   (Luft)"),
         dim);
    line(TextFormat("Zustand: %s   Kombo: %d   Gegner: %d", info.playerState.c_str(), info.comboStep + 1, (int)info.enemyCount), dim);
    line(TextFormat("Kamera: %s   Boxen: %d   Shader: %s", info.flyCamera ? "FLUG" : "folgt", (int)info.boxCount,
                    info.litShader ? "Licht" : "flach (Fehler)"),
         dim);

    // Letzte Log-Zeilen unten links
    const auto& lines = Log::recent();
    int ly = GetScreenHeight() - 14 - (int)lines.size() * 14;
    for (const auto& l : lines) {
        DrawText(l.c_str(), x + 1, ly + 1, 10, Color{0, 0, 0, 200});
        DrawText(l.c_str(), x, ly, 10, Color{220, 225, 235, 255});
        ly += 14;
    }

    // Tastenhilfe unten rechts
    const char* help[] = {"WASD laufen   Leertaste springen   Shift sprinten   Maus umsehen",
                          "Linksklick Angriff (mehrfach = Kombo)   Strg ausweichen (kurz unverwundbar)",
                          "F1 Overlay   F2 Flugkamera (C = runter)   F3 Kollision   F5 alles neu laden   Esc Maus lösen"};
    int hy = GetScreenHeight() - 54;
    for (const char* h : help) {
        int w = MeasureText(h, 10);
        DrawText(h, GetScreenWidth() - w - 11, hy + 1, 10, Color{0, 0, 0, 200});
        DrawText(h, GetScreenWidth() - w - 12, hy, 10, Color{230, 235, 245, 255});
        hy += 14;
    }
}

void DebugOverlay::drawCollision(const Level& level) const {
    for (const LevelBox& b : level.boxes) {
        DrawCubeWiresV(b.center, b.size, b.solid ? Color{255, 90, 90, 255} : Color{90, 200, 255, 255});
    }
    for (const LevelRamp& r : level.ramps) DrawCubeWiresV(r.center, r.size, Color{255, 170, 60, 255});
}

}  // namespace aldoria
