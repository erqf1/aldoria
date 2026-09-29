#include "ui/hud.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace aldoria::hud {
namespace {

void shadowText(const std::string& text, int x, int y, int size, Color c) {
    DrawText(text.c_str(), x + 2, y + 2, size, Color{0, 0, 0, (unsigned char)(c.a * 0.8f)});
    DrawText(text.c_str(), x, y, size, c);
}

void centeredShadowText(const std::string& text, int centerX, int y, int size, Color c) {
    int w = MeasureText(text.c_str(), size);
    shadowText(text, centerX - w / 2, y, size, c);
}

void drawKeyIcon(int x, int y, Color c) {
    DrawCircle(x + 6, y + 7, 6, c);
    DrawCircle(x + 6, y + 7, 2, Color{20, 20, 30, 255});
    DrawRectangle(x + 10, y + 5, 14, 4, c);
    DrawRectangle(x + 19, y + 8, 3, 6, c);
    DrawRectangle(x + 14, y + 8, 3, 4, c);
}

void drawFlaskIcon(int x, int y, bool full) {
    Color glass{190, 220, 236, 255};
    Color liquid = full ? Color{96, 214, 130, 255} : Color{50, 62, 60, 255};
    DrawRectangle(x + 5, y, 6, 6, glass);
    DrawCircle(x + 8, y + 12, 8, glass);
    DrawCircle(x + 8, y + 12, 6, liquid);
}

void drawCooldownIcon(int x, int y, const char* label, float ready, Color color) {
    const int size = 44;
    DrawRectangle(x, y, size, size, Color{14, 16, 26, 210});
    DrawRectangleLinesEx(Rectangle{(float)x, (float)y, (float)size, (float)size}, 2, ready >= 1.0f ? color : Color{90, 92, 110, 255});
    Color fill = color;
    fill.a = ready >= 1.0f ? 255 : 90;
    DrawCircle(x + size / 2, y + size / 2 - 2, 11, fill);
    if (ready < 1.0f) {
        DrawCircleSector(Vector2{(float)x + size / 2, (float)y + size / 2 - 2}, 15, -90.0f, -90.0f + 360.0f * ready, 24, Color{255, 255, 255, 110});
    }
    int w = MeasureText(label, 10);
    DrawText(label, x + (size - w) / 2, y + size - 12, 10, Color{220, 224, 240, 255});
}

}  // namespace

bool projectToScreen(const Camera3D& camera, Vector3 world, Vector2& out) {
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    if (Vector3DotProduct(Vector3Subtract(world, camera.position), forward) < 0.1f) return false;
    out = GetWorldToScreen(world, camera);
    return true;
}

void drawPlayerHealth(const Health& health) {
    const int w = 360, h = 20;
    int x = (GetScreenWidth() - w) / 2;
    int y = GetScreenHeight() - 64;

    DrawRectangle(x - 3, y - 3, w + 6, h + 6, Color{14, 16, 26, 210});
    DrawRectangle(x, y, w, h, Color{52, 30, 34, 255});
    float f = std::clamp(health.fraction(), 0.0f, 1.0f);
    // Bei wenig Leben wird der Balken heller, damit man es auch im Kampf bemerkt
    Color fill = f > 0.3f ? Color{206, 64, 72, 255} : Color{255, 92, 72, 255};
    DrawRectangle(x, y, (int)(w * f), h, fill);
    DrawRectangle(x, y, (int)(w * f), h / 3, Color{255, 255, 255, 40});

    const char* label = TextFormat("%d / %d", (int)std::ceil(health.current()), (int)health.max());
    int tw = MeasureText(label, 20);
    DrawText(label, x + (w - tw) / 2 + 1, y + 1, 20, Color{0, 0, 0, 200});
    DrawText(label, x + (w - tw) / 2, y, 20, WHITE);
}

void drawPlayerHud(const PlayerHudData& d) {
    if (d.health) drawPlayerHealth(*d.health);

    const int cx = GetScreenWidth() / 2;
    const int baseY = GetScreenHeight() - 64;

    // Tränke über der Lebensleiste
    int total = std::min(d.flaskMax, 8);
    int startX = cx - (total * 22) / 2;
    if (d.flaskMax > 0) DrawRectangle(startX - 10, baseY - 42, total * 22 + 52, 34, Color{14, 16, 26, 150});  // lesbar auch vor hellem Himmel
    for (int i = 0; i < total; i++) drawFlaskIcon(startX + i * 22, baseY - 34, i < d.flaskCharges);
    if (d.flaskMax > 0) {
        std::string hint = "R";
        shadowText(hint, startX + total * 22 + 6, baseY - 28, 20, Color{200, 210, 224, 255});
    }

    // Schlüssel und Splitter links unten
    int lx = 18, ly = GetScreenHeight() - 96;
    DrawRectangle(lx - 8, ly - 8, 150, 78, Color{14, 16, 26, 190});
    drawKeyIcon(lx, ly, Color{240, 200, 80, 255});
    shadowText(TextFormat("x %d", d.smallKeys), lx + 34, ly - 2, 20, WHITE);
    if (d.bossKey) {
        drawKeyIcon(lx + 84, ly, Color{200, 100, 240, 255});
    }
    DrawPoly(Vector2{(float)lx + 10, (float)ly + 40}, 4, 9, 0.0f, Color{110, 210, 255, 255});
    shadowText(TextFormat("x %d", d.shards), lx + 34, ly + 28, 20, WHITE);

    // Fähigkeiten rechts unten
    int rx = GetScreenWidth() - 18 - 44;
    int ry = GetScreenHeight() - 70;
    drawCooldownIcon(rx, ry, "Funke", d.sparkReady, Color{255, 170, 60, 255});
    int slot = 1;
    if (d.dashUnlocked) drawCooldownIcon(rx - 54 * slot++, ry, "Dash", d.dashReady, Color{120, 200, 255, 255});
    if (d.doubleJumpUnlocked) drawCooldownIcon(rx - 54 * slot++, ry, "Feder", d.airJumpReady ? 1.0f : 0.0f, Color{200, 240, 255, 255});
}

void drawEnemyOverlays(const Camera3D& camera, const std::vector<Enemy>& enemies) {
    for (const Enemy& e : enemies) {
        if (!e.alive() || e.state() == EnemyState::Spawning) continue;
        Vector2 s;
        Vector3 head{e.position().x, e.position().y + e.height() + 0.45f, e.position().z};
        if (!projectToScreen(camera, head, s)) continue;

        if (e.health().current() < e.health().max()) {
            const int w = 64, h = 7;
            int x = (int)s.x - w / 2, y = (int)s.y;
            DrawRectangle(x - 2, y - 2, w + 4, h + 4, Color{14, 16, 26, 210});
            DrawRectangle(x, y, w, h, Color{52, 30, 34, 255});
            DrawRectangle(x, y, (int)(w * e.health().fraction()), h, Color{226, 92, 82, 255});
        }
        if (e.state() == EnemyState::Telegraph) {
            // Warnzeichen: wächst mit der Vorwarnung
            int size = 30 + (int)(10.0f * e.telegraphProgress());
            int tw = MeasureText("!", size);
            DrawText("!", (int)s.x - tw / 2 + 2, (int)s.y - size - 12, size, Color{0, 0, 0, 220});
            DrawText("!", (int)s.x - tw / 2, (int)s.y - size - 14, size, Color{255, 214, 64, 255});
        }
    }
}

void drawFloatingText(const Camera3D& camera, Vector3 world, const std::string& text, Color color, float alpha) {
    Vector2 s;
    if (!projectToScreen(camera, world, s)) return;
    int a = (int)(255.0f * std::clamp(alpha, 0.0f, 1.0f));
    int tw = MeasureText(text.c_str(), 30);
    DrawText(text.c_str(), (int)s.x - tw / 2 + 2, (int)s.y + 2, 30, Color{0, 0, 0, (unsigned char)(a * 0.8f)});
    DrawText(text.c_str(), (int)s.x - tw / 2, (int)s.y, 30, Color{color.r, color.g, color.b, (unsigned char)a});
}

void drawDamageVignette(float intensity) {
    if (intensity <= 0.01f) return;
    int w = GetScreenWidth(), h = GetScreenHeight();
    int edge = (int)((float)std::min(w, h) * 0.3f);
    unsigned char a = (unsigned char)std::clamp(intensity * 180.0f, 0.0f, 210.0f);
    Color strong{190, 16, 28, a}, none{190, 16, 28, 0};
    DrawRectangleGradientV(0, 0, w, edge, strong, none);
    DrawRectangleGradientV(0, h - edge, w, edge, none, strong);
    DrawRectangleGradientH(0, 0, edge, h, strong, none);
    DrawRectangleGradientH(w - edge, 0, edge, h, none, strong);
}

void drawDeathOverlay(float fade) {
    fade = std::clamp(fade, 0.0f, 1.0f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{40, 6, 10, (unsigned char)(150 * fade)});
    const char* text = "Du wurdest besiegt";
    int size = 40;
    int tw = MeasureText(text, size);
    DrawText(text, (GetScreenWidth() - tw) / 2 + 2, GetScreenHeight() / 2 - 28, size, Color{0, 0, 0, (unsigned char)(220 * fade)});
    DrawText(text, (GetScreenWidth() - tw) / 2, GetScreenHeight() / 2 - 30, size, Color{240, 224, 220, (unsigned char)(255 * fade)});
}

void drawArenaInfo(int wave, int kills, int best, bool bossRush) {
    int w = 230, x = GetScreenWidth() - w - 16, y = 14;
    DrawRectangle(x, y, w, 84, Color{14, 16, 26, 200});
    DrawRectangleLinesEx(Rectangle{(float)x, (float)y, (float)w, 84.0f}, 2, Color{214, 196, 140, 255});
    shadowText(TextFormat(bossRush ? "Boss %d" : "Welle %d", wave), x + 14, y + 8, 30, Color{250, 226, 150, 255});
    shadowText(TextFormat("Besiegt: %d", kills), x + 14, y + 44, 20, WHITE);
    shadowText(TextFormat(bossRush ? "Rekord: Boss %d" : "Rekord: Welle %d", best), x + 14, y + 64, 10, Color{190, 198, 220, 255});
}

void drawBossBar(const std::string& name, float fraction, int phase, bool exhausted) {
    const int w = std::min(760, GetScreenWidth() - 80), h = 22;
    int x = (GetScreenWidth() - w) / 2, y = 54;
    DrawRectangle(x - 4, y - 4, w + 8, h + 8, Color{14, 16, 26, 220});
    DrawRectangle(x, y, w, h, Color{50, 28, 34, 255});
    float f = std::clamp(fraction, 0.0f, 1.0f);
    Color fill = exhausted ? Color{120, 150, 230, 255} : (phase >= 3 ? Color{240, 80, 60, 255} : Color{206, 64, 72, 255});
    DrawRectangle(x, y, (int)(w * f), h, fill);
    DrawRectangle(x, y, (int)(w * f), h / 3, Color{255, 255, 255, 40});
    // Markierungen für die Phasen
    DrawRectangle(x + (int)(w * 0.66f), y, 2, h, Color{255, 255, 255, 120});
    DrawRectangle(x + (int)(w * 0.33f), y, 2, h, Color{255, 255, 255, 120});
    centeredShadowText(name, GetScreenWidth() / 2, y - 34, 20, Color{250, 226, 150, 255});
    if (exhausted) centeredShadowText("Erschöpft: jetzt zuschlagen!", GetScreenWidth() / 2, y + h + 10, 20, Color{160, 200, 255, 255});
}

void drawBanner(const std::string& title, const std::string& subtitle, float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.0f) return;
    unsigned char a = (unsigned char)(255 * alpha);
    int cx = GetScreenWidth() / 2, y = GetScreenHeight() / 5;
    int tw = MeasureText(title.c_str(), 40);
    // Lange Untertitel (z. B. Dungeon-Beschreibung) werden kleiner gesetzt, damit sie ins Fenster passen
    int subSize = 20;
    while (subSize > 12 && MeasureText(subtitle.c_str(), subSize) > GetScreenWidth() - 120) subSize -= 2;
    int sw = subtitle.empty() ? 0 : MeasureText(subtitle.c_str(), subSize);
    int boxW = std::max(tw, sw) + 80;
    DrawRectangle(cx - boxW / 2, y - 12, boxW, subtitle.empty() ? 66 : 96, Color{10, 12, 20, (unsigned char)(150 * alpha)});
    DrawText(title.c_str(), cx - tw / 2 + 2, y + 2, 40, Color{0, 0, 0, (unsigned char)(200 * alpha)});
    DrawText(title.c_str(), cx - tw / 2, y, 40, Color{250, 232, 170, a});
    if (!subtitle.empty()) DrawText(subtitle.c_str(), cx - sw / 2, y + 52, subSize, Color{210, 214, 230, a});
}

void drawPrompt(const std::string& text, Vector2 pos) {
    int w = MeasureText(text.c_str(), 20);
    DrawRectangle((int)pos.x - w / 2 - 10, (int)pos.y - 6, w + 20, 32, Color{14, 16, 26, 210});
    DrawText(text.c_str(), (int)pos.x - w / 2, (int)pos.y, 20, WHITE);
}

void drawFade(float alpha) {
    if (alpha <= 0.0f) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, (unsigned char)(255 * std::clamp(alpha, 0.0f, 1.0f))});
}

void drawSegenChoice(const std::vector<const SegenDef*>& choices, int selected, float time, int shardCost) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{6, 8, 16, 190});
    centeredShadowText("Wähle einen Segen", GetScreenWidth() / 2, 70, 40, Color{250, 232, 170, 255});
    if (shardCost > 0) {
        centeredShadowText(TextFormat("Kostet %d Splitter  ·  Enter: nehmen  ·  Esc: zurück", shardCost), GetScreenWidth() / 2, 120, 20, Color{130, 220, 255, 255});
    } else {
        centeredShadowText("Er begleitet dich auf deiner ganzen Reise.", GetScreenWidth() / 2, 120, 20, Color{200, 206, 226, 255});
    }

    int n = (int)choices.size();
    const int cardW = std::min(300, (GetScreenWidth() - 100) / std::max(1, n) - 20), cardH = 300, gap = 24;
    int totalW = n * cardW + (n - 1) * gap;
    int x0 = (GetScreenWidth() - totalW) / 2;
    int y0 = GetScreenHeight() / 2 - cardH / 2 + 20;

    for (int i = 0; i < n; i++) {
        int x = x0 + i * (cardW + gap);
        bool sel = i == selected;
        int lift = sel ? -10 : 0;
        float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
        DrawRectangle(x, y0 + lift, cardW, cardH, Color{22, 26, 44, 245});
        DrawRectangleLinesEx(Rectangle{(float)x, (float)y0 + lift, (float)cardW, (float)cardH}, sel ? 4.0f : 2.0f,
                             sel ? Color{255, (unsigned char)(200 + 40 * pulse), 110, 255} : Color{110, 112, 150, 255});
        // Symbol: leuchtende Raute
        DrawPoly(Vector2{(float)x + cardW / 2, (float)y0 + lift + 66}, 4, 30, 0.0f, sel ? Color{255, 210, 120, 255} : Color{140, 160, 220, 255});
        DrawPoly(Vector2{(float)x + cardW / 2, (float)y0 + lift + 66}, 4, 18, 0.0f, Color{22, 26, 44, 255});
        centeredShadowText(choices[(size_t)i]->name, x + cardW / 2, y0 + lift + 122, 20, WHITE);

        // Beschreibung mit Zeilenumbruch
        std::string desc = choices[(size_t)i]->description, line;
        int ty = y0 + lift + 164;
        auto flush = [&]() {
            if (line.empty()) return;
            centeredShadowText(line, x + cardW / 2, ty, 20, Color{206, 210, 228, 255});
            ty += 26;
            line.clear();
        };
        std::string word;
        auto pushWord = [&]() {
            if (word.empty()) return;
            std::string test = line.empty() ? word : line + " " + word;
            if (MeasureText(test.c_str(), 20) > cardW - 30 && !line.empty()) {
                flush();
                line = word;
            } else {
                line = test;
            }
            word.clear();
        };
        for (char ch : desc) {
            if (ch == ' ') pushWord();
            else word += ch;
        }
        pushWord();
        flush();
    }
    centeredShadowText("Links/Rechts wählen, Enter bestätigen", GetScreenWidth() / 2, y0 + cardH + 44, 20, Color{180, 188, 214, 255});
}

}  // namespace aldoria::hud
