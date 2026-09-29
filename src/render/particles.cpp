#include "render/particles.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace aldoria {
namespace {

float rnd(float lo, float hi) { return lo + (float)GetRandomValue(0, 10000) / 10000.0f * (hi - lo); }

}  // namespace

void Particles::add(const Particle& p) {
    if (particles_.size() >= kMaxParticles) particles_.erase(particles_.begin());  // ältestes weg
    particles_.push_back(p);
}

void Particles::burst(Vector3 pos, int count, Color color, float speed, float size, float life, float gravity) {
    for (int i = 0; i < count; i++) {
        // Zufällige Richtung (gleichmäßig genug für Effekte)
        Vector3 d{rnd(-1, 1), rnd(-0.2f, 1), rnd(-1, 1)};
        float len = Vector3Length(d);
        if (len < 0.01f) d = {0, 1, 0}, len = 1.0f;
        d = Vector3Scale(d, 1.0f / len);
        float s = speed * rnd(0.4f, 1.0f);
        float l = life * rnd(0.6f, 1.0f);
        add({pos, Vector3Scale(d, s), color, size * rnd(0.6f, 1.2f), l, l, gravity});
    }
}

void Particles::sparkle(Vector3 pos, int count, Color color, float radius) {
    for (int i = 0; i < count; i++) {
        Vector3 offset{rnd(-radius, radius), rnd(0.0f, radius * 1.4f), rnd(-radius, radius)};
        float l = rnd(0.6f, 1.1f);
        add({Vector3Add(pos, offset), {rnd(-0.2f, 0.2f), rnd(0.8f, 1.8f), rnd(-0.2f, 0.2f)}, color, rnd(0.05f, 0.11f), l, l, -0.5f});
    }
}

void Particles::ember(Vector3 pos, Color color) {
    float l = rnd(1.4f, 2.4f);
    add({pos, {rnd(-0.4f, 0.4f), rnd(1.2f, 2.6f), rnd(-0.4f, 0.4f)}, color, rnd(0.09f, 0.17f), l, l, -1.0f});
}

void Particles::dustRing(Vector3 pos, float radius, int count, Color color) {
    for (int i = 0; i < count; i++) {
        float a = 6.2831853f * (float)i / (float)std::max(1, count) + rnd(-0.1f, 0.1f);
        Vector3 d{std::cos(a), 0.0f, std::sin(a)};
        float l = rnd(0.35f, 0.6f);
        add({Vector3Add(pos, Vector3Scale(d, radius * 0.3f)), Vector3Add(Vector3Scale(d, radius * 2.2f), Vector3{0, 0.6f, 0}),
             color, rnd(0.08f, 0.16f), l, l, 2.0f});
    }
}

void Particles::update(float dt) {
    if (dt <= 0.0f) return;
    for (Particle& p : particles_) {
        p.life -= dt;
        p.velocity.y -= p.gravity * dt;
        p.velocity = Vector3Scale(p.velocity, std::exp(-1.5f * dt));  // Luftwiderstand
        p.position = Vector3Add(p.position, Vector3Scale(p.velocity, dt));
    }
    std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.0f; });
}

void Particles::draw(const LitRenderer& r) const {
    for (const Particle& p : particles_) {
        float k = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        Color c = p.color;
        c.a = (unsigned char)(p.color.a * std::min(1.0f, k * 1.6f));
        r.sphere(p.position, p.size * (0.4f + 0.6f * k), c);
    }
}

}  // namespace aldoria
