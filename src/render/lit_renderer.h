#pragma once

#include <functional>
#include <string>
#include <vector>

#include "raylib.h"
#include "render/material_library.h"
#include "render/post_process.h"
#include "world/environment.h"
#include "world/level.h"

namespace aldoria {

// Stoffnetz (Umhang, Banner): kClothCols x kClothRows Punkte, Spalten von links nach rechts (aus Sicht des Trägers von hinten),
// Zeilen von oben nach unten
constexpr int kClothCols = 13;
constexpr int kClothRows = 12;
// Anzahl der vorgefertigten Kegelstumpf-Netze (Verhältnis oberer zu unterem Radius 0 bis 1)
constexpr int kTaperMeshes = 9;

// Ein Punktlicht (Fackel, Feuerschale, Lava, Kristall). Farbe ist Farbe mal Helligkeit, also auch über 1 möglich.
struct PointLight {
    Vector3 position{0, 0, 0};
    Vector3 color{1, 1, 1};
    float radius = 8.0f;
};

// Zeichnet einfache Körper (Box, Kugel, Zylinder, Rampe) mit dem Licht-Shader aus data/shaders:
// Texturen mit Normalkarten, Sonnenschatten, Punktlichter, Nebel. Danach folgt die Nachbearbeitung (Bloom, Tonemapping, FXAA).
// Fällt der Shader aus, wird flach mit dem Standard-Shader gezeichnet und der Fehler geloggt.
class LitRenderer {
public:
    bool init(const std::string& dataDir);
    void shutdown();

    void setEnvironment(const Environment& env) { env_ = env; }
    const Environment& environment() const { return env_; }
    // Thema der Materialien (z. B. "wurzel"); lädt die nötigen Texturen vor, damit es beim Zeichnen nicht ruckelt
    void setTheme(const std::string& theme);
    const std::string& theme() const { return theme_; }
    // 0 niedrig (keine Schatten, kein HDR-Bild), 1 mittel (Schatten, Kantenglättung), 2 hoch (zusätzlich Bloom, feinere Schatten)
    void setQuality(int quality);
    int quality() const { return quality_; }

    void setLights(std::vector<PointLight> lights) { lights_ = std::move(lights); }

    // Rendert ein komplettes Bild in das aktuelle Fenster (innerhalb von BeginDrawing): Schattenkarte, Himmel und Szene
    // ins HDR-Zielbild, dann Nachbearbeitung. `focus` ist der Mittelpunkt des Schattenbereichs (meist der Spieler).
    // `drawScene` zeichnet alle Körper und wird für Schatten und Hauptbild aufgerufen.
    void renderFrame(const Camera3D& camera, Vector3 focus, float time, const std::function<void()>& drawScene);

    // Nur die Licht-Uniforms laden (für Sonderfälle ohne renderFrame)
    void begin(const Camera3D& camera) const;

    // ---- Materialien
    MaterialId material(const std::string& name) const { return lib_.find(name); }
    MaterialId role(const std::string& role) const { return lib_.themeRole(theme_, role); }
    // Alle folgenden Körper bekommen dieses Material (kNoMaterial = einfarbig)
    void useMaterial(MaterialId id) const { curMat_ = id; }
    void clearMaterial() const { curMat_ = kNoMaterial; }
    bool inShadowPass() const { return shadowPass_; }

    void box(Vector3 center, Vector3 size, Color color, float yaw = 0.0f) const;
    void sphere(Vector3 center, float radius, Color color) const;
    void ellipsoid(Vector3 center, Vector3 radii, Color color, float yaw = 0.0f) const;
    // Zylinder steht mit dem Boden bei `baseCenter` und ragt `height` nach oben
    void cylinder(Vector3 baseCenter, float radius, float height, Color color) const;
    void ramp(const LevelRamp& ramp) const;
    // Gliedmaße: Zylinder von a nach b mit runden Enden (Arme, Beine, Rumpf)
    void limb(Vector3 a, Vector3 b, float radius, Color color) const;
    // Verjüngte Gliedmaße: Radius rA bei a, rB bei b (Oberschenkel, Ärmel, Hörner, Zähne). `rounded` setzt Kugeln auf die Enden.
    void taper(Vector3 a, Vector3 b, float rA, float rB, Color color, bool rounded = true) const;
    // Flacher Balken von a nach b mit Querschnitt (Breite x Dicke), `up` legt fest, wohin die Breite zeigt (Schwertklinge, Bogen)
    void bar(Vector3 a, Vector3 b, float width, float thickness, Color color, Vector3 up = {0, 1, 0}) const;
    // Texturen haften an dieser Figur (Ort und Blickrichtung), bis clearAnchor() aufgerufen wird
    void setAnchor(Vector3 position, float yaw) const;
    void clearAnchor() const;
    // Zweiseitiges, weich schattiertes Stoffnetz aus kClothCols * kClothRows Punkten (Zeile für Zeile)
    void cloth(const Vector3* grid, Color color) const;

    bool usingLitShader() const { return lit_; }
    bool hdrActive() const { return post_.ready() && quality_ >= 1; }

private:
    void draw(const Mesh& mesh, Vector3 scale, float yaw, Vector3 position, Color color) const;
    void drawMatrix(const Mesh& mesh, const Matrix& m, Color color) const;
    void bindSurface(Color color) const;
    void uploadMaterial(MaterialDef* def, MaterialId id) const;
    void renderShadowMap(Vector3 focus, const std::function<void()>& drawScene);
    void drawSky(const Camera3D& camera, float time, int width, int height, bool directOut) const;
    void uploadFrameUniforms(const Camera3D& camera, float time, bool directOut) const;
    bool ensureShadowTarget(int size);
    void destroyShadowTarget();

    bool ready_ = false;
    bool lit_ = false;
    Shader shader_{}, depthShader_{}, skyShader_{};
    Material material_{}, depthMaterial_{};
    Texture2D whiteTex_{};
    Mesh cube_{}, sphere_{}, cylinder_{}, wedge_{}, cloth_{};
    Mesh taper_[kTaperMeshes]{};
    Environment env_;
    std::string theme_ = "wurzel";
    int quality_ = 2;
    mutable MaterialLibrary lib_;
    PostProcess post_;
    std::vector<PointLight> lights_;

    // Schattenkarte
    RenderTexture2D shadowRT_{};
    int shadowSize_ = 0;
    bool shadowPass_ = false;
    bool shadowValid_ = false;
    Matrix lightVP_{};

    // Zustand des Materials beim Zeichnen (wird in const-Zeichenfunktionen zwischengespeichert)
    mutable MaterialId curMat_ = kNoMaterial;
    mutable bool anchored_ = false;
    mutable Vector4 anchor_{0, 0, 0, 0};
    mutable int lastAnchored_ = -1;
    mutable MaterialId lastMat_ = -2;

    // Uniform-Orte des Licht-Shaders
    int locViewPos_ = -1, locSunDir_ = -1, locSunColor_ = -1, locSkyAmbient_ = -1;
    int locGroundAmbient_ = -1, locFogColor_ = -1, locFogStart_ = -1, locFogEnd_ = -1;
    int locUseAnchor_ = -1, locTexAnchor_ = -1, locUseTex_ = -1, locMatA_ = -1, locMatB_ = -1, locMatC_ = -1, locHasNormal_ = -1, locTime_ = -1, locGroundY_ = -1;
    int locAmbientBoost_ = -1, locSunBoost_ = -1;
    int locLightVP_ = -1, locShadowTexel_ = -1, locShadowOn_ = -1;
    int locLightCount_ = -1, locLightPos_ = -1, locLightCol_ = -1;
    int locDirectOut_ = -1, locExposure_ = -1;
    // Himmel-Shader
    int skyRes_ = -1, skyF_ = -1, skyR_ = -1, skyU_ = -1, skyTan_ = -1, skyAspect_ = -1, skyTop_ = -1, skyHorizon_ = -1;
    int skySunDir_ = -1, skySunColor_ = -1, skyTime_ = -1, skyCloud_ = -1, skyGlow_ = -1, skyDirect_ = -1, skyExposure_ = -1;
};

// Setzt ein Material für einen Zeichenabschnitt und stellt danach "einfarbig" wieder her
class ScopedMaterial {
public:
    ScopedMaterial(const LitRenderer& r, MaterialId id) : r_(r) { r_.useMaterial(id); }
    ~ScopedMaterial() { r_.clearMaterial(); }
    ScopedMaterial(const ScopedMaterial&) = delete;
    ScopedMaterial& operator=(const ScopedMaterial&) = delete;

private:
    const LitRenderer& r_;
};

}  // namespace aldoria
