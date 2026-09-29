#pragma once

#include <optional>
#include <string>
#include <vector>

#include "combat/melee.h"
#include "components/health.h"
#include "core/json.h"
#include "raylib.h"
#include "render/lit_renderer.h"
#include "world/kinematic_body.h"
#include "world/level.h"

namespace aldoria {

// Werte eines Bosses aus data/enemies/<id>.json. Zeiten in Sekunden, Winkel in Radiant (JSON: Grad).
struct BossDef {
    std::string id = "boss";
    std::string name = "Boss";
    std::string style = "king";   // "king" (Krone) oder "golem" (Stein mit glühendem Kern)
    std::string introText = "Besiege den Herrscher der Wurzelhallen!";
    std::string phase2Text = "Der König ruft seine Diener!";
    std::string phase3Text = "Der König tobt vor Wut!";
    std::string arenaText = "Der Kobold-König fordert dich heraus.";
    float maxHealth = 320.0f;
    float radius = 1.25f;
    float height = 3.0f;
    float moveSpeed = 2.6f;
    Color color{176, 72, 56, 255};
    float phase2At = 0.66f;   // Anteil der Lebenspunkte, ab dem Phase 2 beginnt
    float phase3At = 0.33f;
    float phaseChangeTime = 1.6f;

    // Hieb
    float swingTelegraph = 0.9f, swingActive = 0.25f, swingRecovery = 1.0f;
    float swingRange = 3.6f, swingHalfAngle = 1.4f, swingDamage = 22.0f, swingKnockback = 10.0f, swingLunge = 5.0f;
    // Stampfen (Phase 1 und 2: Flächenschaden; Phase 3: Schockwelle, über die man springen muss)
    float stompTelegraph = 1.1f, stompRadius = 5.5f, stompDamage = 20.0f, stompRecovery = 1.3f;
    float waveSpeed = 9.0f, waveMaxRadius = 15.0f, waveDamage = 16.0f;
    // Feuersalve
    float volleyTelegraph = 0.8f, volleySpeed = 12.0f, volleyDamage = 12.0f, volleyRecovery = 0.9f, volleySpread = 0.5f;
    int volleyCount = 3;
    // Beschwören
    float summonTelegraph = 1.2f, summonCooldown = 16.0f;
    int summonCount = 2;
    std::string summonType = "forest_imp";
    // Sprungangriff
    float leapTelegraph = 0.9f, leapTime = 0.75f, leapHeight = 4.5f, leapRadius = 3.6f, leapDamage = 24.0f;
    // Ansturm: der Boss rennt in gerader Linie los (Vorwarnung zeigt die Bahn), gegen eine Wand macht ihn das müde
    float chargeTelegraph = 1.0f, chargeSpeed = 14.0f, chargeTime = 1.0f, chargeDamage = 24.0f, chargeKnockback = 12.0f;
    // Meteoren: rote Kreise am Boden schlagen nach kurzer Verzögerung ein
    float meteorTelegraph = 1.0f, meteorDelay = 1.3f, meteorRadius = 2.3f, meteorDamage = 22.0f;
    int meteorCount = 5;
    // Spiralfeuer: der Boss dreht sich und schießt Feuerbälle in Brusthöhe rundherum (ausweichen mit Strg oder springen)
    float spiralTelegraph = 0.9f, spiralDuration = 2.6f, spiralInterval = 0.13f, spiralSpeed = 9.5f, spiralDamage = 11.0f, spiralTurn = 2.4f;
    int spiralArms = 3;
    // Funken (Fernkampf des Spielers) richten nur einen Teil des Schadens an; wer nur aus der Ferne schießt, wird angerannt
    float sparkResist = 0.5f;
    // Erschöpfung nach schweren Angriffen: hier ist der Boss verwundbar
    float exhaustedTime = 1.8f, exhaustedDamageMult = 1.5f;

    static BossDef fromJson(const json& j);
    static std::optional<BossDef> loadFromFile(const std::string& path);
};

enum class BossState { Waiting, Chase, Telegraph, Attack, Recover, Exhausted, PhaseChange, Dead };
enum class BossAttack { None, Swing, Stomp, Volley, Summon, Leap, Charge, Meteors, Spiral };

// Ergebnisse von Angriffen, die das Spiel auswertet (Schaden, Projektile, Gegner rufen)
struct BossEvent {
    enum class Type { Stomp, Shockwave, Volley, Summon, LeapLand, PhaseChange, Meteors, SpiralShot, ChargeCrash };
    Type type = Type::Stomp;
    Vector3 position{0, 0, 0};
    Vector3 direction{0, 0, 1};
    Vector3 target{0, 0, 0};   // Zielpunkt (Brusthöhe des Spielers), z. B. für gezielte Feuerbälle
    float radius = 0.0f;
    float damage = 0.0f;
    float speed = 0.0f;
    int count = 0;
    int phase = 1;
};

// Boss mit Phasen: Jeder Angriff hat eine lesbare Vorwarnung, schwere Angriffe machen ihn müde und verwundbar.
class Boss {
public:
    Boss(const BossDef* def, Vector3 spawn);

    void start() { if (state_ == BossState::Waiting) state_ = BossState::Chase; }
    void update(float dt, const Level& level, Vector3 playerPos, bool playerAlive);
    float takeHit(float damage, Vector3 knockback);

    // Hieb: der Spielcode prüft, ob der Spieler im swingQuery() steht
    // Ansturm: der Spielcode prüft die Berührung mit dem Spieler
    bool chargeHitActive() const { return state_ == BossState::Attack && attack_ == BossAttack::Charge && !chargeConsumed_; }
    void consumeCharge() { chargeConsumed_ = true; }
    Vector3 chargeDirection() const { return chargeDir_; }
    bool swingHitActive() const { return state_ == BossState::Attack && attack_ == BossAttack::Swing && !swingConsumed_; }
    void consumeSwing() { swingConsumed_ = true; }
    MeleeQuery swingQuery() const;
    std::vector<BossEvent> takeEvents();

    void resetAggro();
    void draw(const LitRenderer& renderer, const Level& level) const;

    KinematicBody& body() { return body_; }
    Vector3 position() const { return body_.position; }
    float radius() const { return def_->radius; }
    float height() const { return def_->height; }
    float yaw() const { return yaw_; }
    const BossDef& def() const { return *def_; }
    const Health& health() const { return health_; }
    BossState state() const { return state_; }
    BossAttack attack() const { return attack_; }
    int phase() const { return phase_; }
    bool alive() const { return state_ != BossState::Dead; }
    bool started() const { return state_ != BossState::Waiting; }
    bool targetable() const { return state_ != BossState::Dead && state_ != BossState::PhaseChange && state_ != BossState::Waiting; }
    bool removable() const { return state_ == BossState::Dead && deadTimer_ >= kDeathDuration; }
    bool exhausted() const { return state_ == BossState::Exhausted; }
    float telegraphProgress() const;

    int lastHitSwing = -1;

private:
    static constexpr float kDeathDuration = 2.2f;
    float phaseSpeed() const { return phase_ >= 3 ? 0.8f : (phase_ == 2 ? 0.9f : 1.0f); }
    void chooseAttack(float dist);
    void beginTelegraph(BossAttack a);
    void finishTelegraph(Vector3 playerPos);
    void enterPhase(int phase);
    void brake(float dt, float rate);

    const BossDef* def_;
    KinematicBody body_;
    Health health_;
    BossState state_ = BossState::Waiting;
    BossAttack attack_ = BossAttack::None;
    int phase_ = 1;
    int pendingPhase_ = 0;
    float timer_ = 0.0f;
    float telegraphTotal_ = 1.0f;
    float deadTimer_ = 0.0f;
    float flashTimer_ = 0.0f;
    float yaw_ = 0.0f;
    float decisionDelay_ = 1.0f;
    float summonTimer_ = 6.0f;
    bool swingConsumed_ = false;

    // Ansturm, Spiralfeuer, Wahl der Angriffe
    Vector3 chargeDir_{0, 0, 1};
    Vector3 aimPoint_{0, 0, 0};
    bool chargeConsumed_ = false;
    float chargeAge_ = 0.0f;
    float spiralAngle_ = 0.0f, spiralTick_ = 0.0f, spiralSign_ = 1.0f;
    BossAttack lastAttack_ = BossAttack::None;
    float farTimer_ = 0.0f;   // wie lange der Spieler schon weit weg ist
    float chargeTelegraphLen() const { return def_->chargeSpeed * def_->chargeTime; }

    // Sprung
    Vector3 leapStart_{0, 0, 0};
    Vector3 leapTarget_{0, 0, 0};
    float leapT_ = 0.0f;

    std::vector<BossEvent> events_;
};

}  // namespace aldoria
