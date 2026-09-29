#pragma once

#include <vector>

#include "raylib.h"
#include "render/lit_renderer.h"

namespace aldoria {

// Einfache Teilchen (kleine Kugeln) für Treffer, Staub, Funkeln und Explosionen.
class Particles {
public:
    // Ungerichteter Ausbruch
    void burst(Vector3 position, int count, Color color, float speed, float size, float life, float gravity = 9.0f);
    // Teilchen steigen langsam auf (Sammeln, Heilen, Ruheplatz)
    void sparkle(Vector3 position, int count, Color color, float radius = 0.6f);
    // Glühende Funken, die langsam aufsteigen (Lava, Feuerschalen)
    void ember(Vector3 position, Color color);
    // Ringförmige Staubwolke am Boden (Landung, Stampfen)
    void dustRing(Vector3 position, float radius, int count, Color color);

    void update(float dt);
    void draw(const LitRenderer& renderer) const;
    void clear() { particles_.clear(); }
    size_t count() const { return particles_.size(); }

private:
    struct Particle {
        Vector3 position;
        Vector3 velocity;
        Color color;
        float size;
        float life;
        float maxLife;
        float gravity;
    };
    void add(const Particle& p);

    static constexpr size_t kMaxParticles = 700;
    std::vector<Particle> particles_;
};

}  // namespace aldoria
