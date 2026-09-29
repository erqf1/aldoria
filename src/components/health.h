#pragma once

#include <algorithm>

namespace aldoria {

// Wiederverwendbare Lebenspunkte für Spieler und Gegner.
class Health {
public:
    Health() = default;
    explicit Health(float maxHealth) : max_(maxHealth), current_(maxHealth) {}

    float current() const { return current_; }
    float max() const { return max_; }
    float fraction() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }
    bool dead() const { return current_ <= 0.0f; }
    bool invulnerable() const { return invulnerableTimer_ > 0.0f; }

    void setMax(float newMax, bool refill) {
        max_ = newMax;
        current_ = refill ? newMax : std::min(current_, newMax);
    }
    void refill() {
        current_ = max_;
        invulnerableTimer_ = 0.0f;
    }
    void grantInvulnerability(float seconds) { invulnerableTimer_ = std::max(invulnerableTimer_, seconds); }
    void update(float dt) { invulnerableTimer_ = std::max(0.0f, invulnerableTimer_ - dt); }

    // Liefert den tatsächlich verlorenen Wert (0 bei Unverwundbarkeit oder wenn schon tot)
    float damage(float amount) {
        if (dead() || invulnerable() || amount <= 0.0f) return 0.0f;
        float actual = amount * (1.0f - std::clamp(resistance, 0.0f, 0.9f));
        actual = std::min(actual, current_);
        current_ -= actual;
        return actual;
    }

    // Liefert die tatsächlich geheilte Menge
    float heal(float amount) {
        if (dead() || amount <= 0.0f) return 0.0f;
        float actual = std::min(amount, max_ - current_);
        current_ += actual;
        return actual;
    }

    float resistance = 0.0f;  // Anteil (0 bis 0,9), der vom Schaden abgezogen wird

private:
    float max_ = 100.0f;
    float current_ = 100.0f;
    float invulnerableTimer_ = 0.0f;
};

}  // namespace aldoria
