#include "combat/projectile.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace aldoria {

void ProjectileSystem::update(float dt, const Level& level) {
    if (dt <= 0.0f) return;
    for (Projectile& p : projectiles_) {
        if (p.dead) continue;
        p.life -= dt;
        if (p.life <= 0.0f) {
            p.dead = true;
            continue;
        }
        p.velocity.y -= p.gravity * dt;

        Vector3 step = Vector3Scale(p.velocity, dt);
        float dist = Vector3Length(step);
        if (dist > 1e-5f) {
            Vector3 dir = Vector3Scale(step, 1.0f / dist);
            float hit;
            if (level.raycast(p.position, dir, dist + p.radius, hit)) {
                impacts_.push_back(Vector3Add(p.position, Vector3Scale(dir, std::max(0.0f, hit - p.radius))));
                p.dead = true;
                continue;
            }
        }
        p.position = Vector3Add(p.position, step);
        if (p.position.y < level.killY) p.dead = true;
        // Boden trifft man auch
        if (level.hasGround && p.position.y - p.radius < level.groundY) {
            impacts_.push_back({p.position.x, level.groundY, p.position.z});
            p.dead = true;
        }
    }
    std::erase_if(projectiles_, [](const Projectile& p) { return p.dead; });
}

std::vector<Vector3> ProjectileSystem::takeImpacts() {
    std::vector<Vector3> out;
    out.swap(impacts_);
    return out;
}

void ProjectileSystem::draw(const LitRenderer& r) const {
    float t = (float)GetTime();
    for (const Projectile& p : projectiles_) {
        float pulse = 1.0f + 0.15f * std::sin(t * 30.0f + p.position.x);
        Color glow = p.color;
        glow.a = 110;
        r.sphere(p.position, p.radius * 1.9f * pulse, glow);
        r.sphere(p.position, p.radius * pulse, p.color);
    }
}

}  // namespace aldoria
