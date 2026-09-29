#pragma once

#include <string>
#include <vector>

#include "combat/player_modifiers.h"
#include "components/health.h"
#include "entities/enemy.h"
#include "raylib.h"

namespace aldoria::hud {

// Weltposition auf den Bildschirm. Gibt false zurück, wenn der Punkt hinter der Kamera liegt.
bool projectToScreen(const Camera3D& camera, Vector3 world, Vector2& out);

struct PlayerHudData {
    const Health* health = nullptr;
    int flaskCharges = 0;
    int flaskMax = 0;
    int smallKeys = 0;
    bool bossKey = false;
    int shards = 0;
    bool dashUnlocked = false;
    bool doubleJumpUnlocked = false;
    bool airJumpReady = true;
    float dashReady = 1.0f;   // 0..1
    float sparkReady = 1.0f;  // 0..1
};

// Leben, Tränke, Schlüssel, Splitter und Fähigkeiten
void drawPlayerHud(const PlayerHudData& data);
void drawPlayerHealth(const Health& health);
// Lebensbalken über verletzten Gegnern und "!"-Warnung, wenn ein Angriff kommt
void drawEnemyOverlays(const Camera3D& camera, const std::vector<Enemy>& enemies);
void drawFloatingText(const Camera3D& camera, Vector3 world, const std::string& text, Color color, float alpha);
// fade: 0..1
void drawDeathOverlay(float fade);
// Roter Rand bei Treffern und niedrigem Leben. intensity: 0..1
void drawDamageVignette(float intensity);

// Anzeige oben rechts im Endlos-Arena-Modus
void drawArenaInfo(int wave, int kills, int best, bool bossRush = false);

void drawBossBar(const std::string& name, float fraction, int phase, bool exhausted);
// Große Einblendung oben in der Mitte (Raumname, "Kampf!", ...). alpha: 0..1
void drawBanner(const std::string& title, const std::string& subtitle, float alpha);
void drawPrompt(const std::string& text, Vector2 screenPos);
void drawFade(float alpha);

// Auswahl von drei Segen; `selected` ist der markierte Index
// shardCost > 0: am Ruheplatz gekauft (kostet Splitter, Esc bricht ab)
void drawSegenChoice(const std::vector<const SegenDef*>& choices, int selected, float time, int shardCost = 0);

}  // namespace aldoria::hud
