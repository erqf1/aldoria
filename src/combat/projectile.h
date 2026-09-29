#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "render/lit_renderer.h"
#include "world/level.h"

namespace aldoria {

struct Projectile {
    Vector3 position{0, 0, 0};
    Vector3 velocity{0, 0, 0};
    float radius = 0.28f;
    float damage = 10.0f;
    float knockback = 5.0f;
    float life = 3.0f;
    float gravity = 0.0f;
    bool fromPlayer = false;
    bool dead = false;
    Color color{255, 170, 60, 255};
};

// Bewegt Projektile und lässt sie an Wänden enden. Treffer auf Figuren wertet das Spiel aus,
// weil dafür Gegner, Boss und Spieler bekannt sein müssen.
class ProjectileSystem {
public:
    void spawn(const Projectile& p) { projectiles_.push_back(p); }
    void update(float dt, const Level& level);
    void clear() {
        projectiles_.clear();
        impacts_.clear();
    }

    std::vector<Projectile>& all() { return projectiles_; }
    const std::vector<Projectile>& all() const { return projectiles_; }

    // Orte, an denen Projektile eine Wand getroffen haben (für Funken); wird beim Abholen geleert
    std::vector<Vector3> takeImpacts();

    void draw(const LitRenderer& renderer) const;

private:
    std::vector<Projectile> projectiles_;
    std::vector<Vector3> impacts_;
};

}  // namespace aldoria
