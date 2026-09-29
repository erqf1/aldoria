#pragma once

#include <string>

#include "combat/melee.h"
#include "components/health.h"
#include "entities/enemy_def.h"
#include "raylib.h"
#include "render/lit_renderer.h"
#include "world/kinematic_body.h"
#include "world/level.h"

namespace aldoria {

enum class EnemyState { Spawning, Idle, Chase, Telegraph, Attack, Recover, Hurt, Dead };

// Gegner mit lesbarem Ablauf: verfolgen, Vorwarnung (Telegraph), Angriff, Erholung.
// Verhalten (Nahkampf, Fernkampf, Sprung) kommt aus den Daten.
class Enemy {
public:
    // `spawnDelay` > 0: der Gegner taucht erst auf (unverwundbar und untätig), z. B. bei Arena-Wellen
    Enemy(const EnemyDef* def, Vector3 spawn, float spawnDelay = 0.0f);

    void update(float dt, const Level& level, Vector3 playerPos, bool playerAlive);

    // Verarbeitet einen Treffer. `knockback` ist ein horizontaler Geschwindigkeitsstoß.
    // Gibt den tatsächlichen Schaden zurück.
    float takeHit(float damage, Vector3 knockback);

    // Trefferphase des Angriffs: der Spielcode prüft dann, ob der Spieler im attackQuery() steht
    bool attackHitActive() const { return state_ == EnemyState::Attack && !attackConsumed_; }
    void consumeAttack() { attackConsumed_ = true; }
    MeleeQuery attackQuery() const;

    // Fernkämpfer: wurde geschossen? Liefert Startpunkt und Richtung einmalig.
    bool consumeShot(Vector3& origin, Vector3& direction);

    void resetAggro();
    void draw(const LitRenderer& renderer, const Level& level) const;

    KinematicBody& body() { return body_; }
    Vector3 position() const { return body_.position; }
    float radius() const { return def_->radius; }
    float height() const { return def_->height; }
    float yaw() const { return yaw_; }
    const EnemyDef& def() const { return *def_; }
    const Health& health() const { return health_; }
    EnemyState state() const { return state_; }
    // Lebt und ist im Spiel (auch beim Auftauchen)
    bool alive() const { return state_ != EnemyState::Dead; }
    // Kann getroffen werden
    bool targetable() const { return state_ != EnemyState::Dead && state_ != EnemyState::Spawning; }
    bool removable() const { return state_ == EnemyState::Dead && deadTimer_ >= kDeathDuration; }
    // 0..1: Fortschritt der Vorwarnung
    float telegraphProgress() const;

    // Jeder Schlag des Spielers hat eine eigene Nummer, damit er einen Gegner nur einmal trifft
    int lastHitSwing = -1;
    // Stand schon beim Laden im Raum (im Gegensatz zu Wellen und Beschworenen); zählt für "Raum gesäubert"
    bool isStatic = false;

private:
    static constexpr float kDeathDuration = 0.8f;
    void brake(float dt, float rate);
    // Am Boden nie über eine Kante ins Leere laufen; Absturz-Notbremse
    void guardEdges(const Level& level);
    void rescueIfFallen(const Level& level);
    // Trägt der Sprung in Blickrichtung `yaw` bis auf festen Boden?
    bool leapSafe(const Level& level, float yaw) const;
    void updateChase(float dt, const Level& level, Vector3 playerPos, Vector3 dir, float dist, float yawToPlayer);

    const EnemyDef* def_;
    KinematicBody body_;
    Health health_;
    EnemyState state_ = EnemyState::Idle;
    float timer_ = 0.0f;
    float deadTimer_ = 0.0f;
    float flashTimer_ = 0.0f;
    float poiseCooldown_ = 0.0f;  // nach einer Betäubung kurz nicht erneut betäubbar (kein Dauer-Stun)
    float yaw_ = 0.0f;
    bool attackConsumed_ = false;

    // Fernkampf / Sprung
    float shootTimer_ = 1.0f;
    float walkPhase_ = 0.0f;   // Laufanimation
    float leapTimer_ = 1.0f;
    float strafeTimer_ = 0.0f;
    float strafeDir_ = 1.0f;
    float spawnDuration_ = 0.0f;
    bool shotPending_ = false;
    Vector3 shotOrigin_{0, 0, 0};
    Vector3 shotDirection_{0, 0, 1};

    // Absturzschutz
    Vector3 spawnPos_{0, 0, 0};
    Vector3 lastSafe_{0, 0, 0};
    int rescues_ = 0;
};

}  // namespace aldoria
