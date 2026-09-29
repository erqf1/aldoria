#pragma once

#include <string>

#include "raylib.h"
#include "world/environment.h"

namespace aldoria {

// HDR-Bild der Szene plus Nachbearbeitung: Bloom (Leuchten), Tonemapping, Kantenglättung, Farbstimmung.
class PostProcess {
public:
    bool init(const std::string& shaderDir);
    void shutdown();
    bool ready() const { return ready_; }

    // Passt die Zielbilder an die Fenstergröße an (bei Änderung neu anlegen)
    bool resize(int width, int height);
    RenderTexture2D& scene() { return scene_; }

    // Zeichnet das fertige Bild in den aktuellen Framebuffer (das Fenster). quality: 0 aus, 1 nur Glättung, 2 alles
    void present(const Environment& env, float time, int quality);

private:
    static RenderTexture2D makeTarget(int width, int height, bool withDepth);
    void destroyTargets();
    void runBloom(const Environment& env);

    static constexpr int kBloomLevels = 5;

    bool ready_ = false;
    int width_ = 0, height_ = 0;
    RenderTexture2D scene_{};
    RenderTexture2D bloom_[kBloomLevels]{};
    Shader down_{}, up_{}, final_{};
    int locDownTexel_ = -1, locDownFirst_ = -1, locDownThreshold_ = -1;
    int locUpTexel_ = -1, locUpRadius_ = -1;
    int locFinalBloom_ = -1, locFinalTexel_ = -1, locExposure_ = -1, locBloomStrength_ = -1, locVignette_ = -1;
    int locSaturation_ = -1, locGrade_ = -1, locFxaa_ = -1, locTime_ = -1;
};

}  // namespace aldoria
