#include "entities/boss.h"

#include <algorithm>
#include <cmath>

#include "core/file_util.h"
#include "core/log.h"
#include "core/math_util.h"
#include "raymath.h"

namespace aldoria {
namespace {

constexpr float kGravity = 28.0f;
constexpr float kMaxFall = 40.0f;

Color lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), 255};
}

float randomUnit() { return (float)GetRandomValue(0, 10000) / 10000.0f; }

}  // namespace

// ---------------------------------------------------------------- Daten

BossDef BossDef::fromJson(const json& j) {
    BossDef d;
    if (!j.is_object()) return d;
    d.id = j.value("id", d.id);
    d.name = j.value("name", d.name);
    d.style = j.value("style", d.style);
    d.introText = j.value("intro_text", d.introText);
    d.phase2Text = j.value("phase2_text", d.phase2Text);
    d.phase3Text = j.value("phase3_text", d.phase3Text);
    d.arenaText = j.value("arena_text", d.arenaText);
    d.maxHealth = j.value("max_health", d.maxHealth);
    d.radius = j.value("radius", d.radius);
    d.height = j.value("height", d.height);
    d.moveSpeed = j.value("move_speed", d.moveSpeed);
    d.phase2At = j.value("phase2_at", d.phase2At);
    d.phase3At = j.value("phase3_at", d.phase3At);
    d.phaseChangeTime = j.value("phase_change_time", d.phaseChangeTime);
    d.swingTelegraph = j.value("swing_telegraph", d.swingTelegraph);
    d.swingActive = j.value("swing_active", d.swingActive);
    d.swingRecovery = j.value("swing_recovery", d.swingRecovery);
    d.swingRange = j.value("swing_range", d.swingRange);
    d.swingHalfAngle = j.value("swing_half_angle_deg", d.swingHalfAngle * 57.2957795f) * 0.0174532925f;
    d.swingDamage = j.value("swing_damage", d.swingDamage);
    d.swingKnockback = j.value("swing_knockback", d.swingKnockback);
    d.swingLunge = j.value("swing_lunge", d.swingLunge);
    d.stompTelegraph = j.value("stomp_telegraph", d.stompTelegraph);
    d.stompRadius = j.value("stomp_radius", d.stompRadius);
    d.stompDamage = j.value("stomp_damage", d.stompDamage);
    d.stompRecovery = j.value("stomp_recovery", d.stompRecovery);
    d.waveSpeed = j.value("wave_speed", d.waveSpeed);
    d.waveMaxRadius = j.value("wave_max_radius", d.waveMaxRadius);
    d.waveDamage = j.value("wave_damage", d.waveDamage);
    d.volleyTelegraph = j.value("volley_telegraph", d.volleyTelegraph);
    d.volleyCount = j.value("volley_count", d.volleyCount);
    d.volleySpread = j.value("volley_spread", d.volleySpread);
    d.volleySpeed = j.value("volley_speed", d.volleySpeed);
    d.volleyDamage = j.value("volley_damage", d.volleyDamage);
    d.volleyRecovery = j.value("volley_recovery", d.volleyRecovery);
    d.summonTelegraph = j.value("summon_telegraph", d.summonTelegraph);
    d.summonCount = j.value("summon_count", d.summonCount);
    d.summonCooldown = j.value("summon_cooldown", d.summonCooldown);
    d.summonType = j.value("summon_type", d.summonType);
    d.leapTelegraph = j.value("leap_telegraph", d.leapTelegraph);
    d.leapTime = j.value("leap_time", d.leapTime);
    d.leapHeight = j.value("leap_height", d.leapHeight);
    d.leapRadius = j.value("leap_radius", d.leapRadius);
    d.leapDamage = j.value("leap_damage", d.leapDamage);
    d.chargeTelegraph = j.value("charge_telegraph", d.chargeTelegraph);
    d.chargeSpeed = j.value("charge_speed", d.chargeSpeed);
    d.chargeTime = j.value("charge_time", d.chargeTime);
    d.chargeDamage = j.value("charge_damage", d.chargeDamage);
    d.chargeKnockback = j.value("charge_knockback", d.chargeKnockback);
    d.meteorTelegraph = j.value("meteor_telegraph", d.meteorTelegraph);
    d.meteorDelay = j.value("meteor_delay", d.meteorDelay);
    d.meteorRadius = j.value("meteor_radius", d.meteorRadius);
    d.meteorDamage = j.value("meteor_damage", d.meteorDamage);
    d.meteorCount = j.value("meteor_count", d.meteorCount);
    d.spiralTelegraph = j.value("spiral_telegraph", d.spiralTelegraph);
    d.spiralDuration = j.value("spiral_duration", d.spiralDuration);
    d.spiralInterval = j.value("spiral_interval", d.spiralInterval);
    d.spiralSpeed = j.value("spiral_speed", d.spiralSpeed);
    d.spiralDamage = j.value("spiral_damage", d.spiralDamage);
    d.spiralTurn = j.value("spiral_turn", d.spiralTurn);
    d.spiralArms = j.value("spiral_arms", d.spiralArms);
    d.sparkResist = j.value("spark_resist", d.sparkResist);
    d.exhaustedTime = j.value("exhausted_time", d.exhaustedTime);
    d.exhaustedDamageMult = j.value("exhausted_damage_mult", d.exhaustedDamageMult);
    if (auto it = j.find("color"); it != j.end() && it->is_array() && it->size() >= 3) {
        auto c = [&](size_t i) { return (unsigned char)std::clamp((*it)[i].get<int>(), 0, 255); };
        d.color = {c(0), c(1), c(2), 255};
    }
    return d;
}

std::optional<BossDef> BossDef::loadFromFile(const std::string& path) {
    std::string error;
    auto j = loadJsonFile(path, error);
    if (!j) {
        Log::error(LogCategory::Loading, "Boss konnte nicht geladen werden: {}", error);
        return std::nullopt;
    }
    return fromJson(*j);
}

// ---------------------------------------------------------------- Verhalten

Boss::Boss(const BossDef* def, Vector3 spawn) : def_(def), health_(def->maxHealth) {
    body_.position = spawn;
    body_.radius = def->radius;
    body_.height = def->height;
    summonTimer_ = std::min(6.0f, def->summonCooldown);
}

void Boss::brake(float dt, float rate) {
    body_.velocity.x = approach(body_.velocity.x, 0.0f, rate * dt);
    body_.velocity.z = approach(body_.velocity.z, 0.0f, rate * dt);
}

float Boss::telegraphProgress() const {
    if (state_ != BossState::Telegraph || telegraphTotal_ <= 0.0f) return 0.0f;
    return std::clamp(1.0f - timer_ / telegraphTotal_, 0.0f, 1.0f);
}

MeleeQuery Boss::swingQuery() const {
    return {body_.position, yaw_, def_->swingRange + def_->radius, def_->swingHalfAngle};
}

std::vector<BossEvent> Boss::takeEvents() {
    std::vector<BossEvent> out;
    out.swap(events_);
    return out;
}

void Boss::resetAggro() {
    if (state_ == BossState::Dead || state_ == BossState::Waiting) return;
    state_ = BossState::Waiting;
    attack_ = BossAttack::None;
    body_.velocity.x = body_.velocity.z = 0.0f;
}

void Boss::beginTelegraph(BossAttack a) {
    attack_ = a;
    state_ = BossState::Telegraph;
    float base = 1.0f;
    switch (a) {
        case BossAttack::Swing: base = def_->swingTelegraph; break;
        case BossAttack::Stomp: base = def_->stompTelegraph; break;
        case BossAttack::Volley: base = def_->volleyTelegraph; break;
        case BossAttack::Summon: base = def_->summonTelegraph; break;
        case BossAttack::Leap: base = def_->leapTelegraph; break;
        case BossAttack::Charge: base = def_->chargeTelegraph; break;
        case BossAttack::Meteors: base = def_->meteorTelegraph; break;
        case BossAttack::Spiral: base = def_->spiralTelegraph; break;
        case BossAttack::None: break;
    }
    lastAttack_ = a;
    telegraphTotal_ = timer_ = base * phaseSpeed();
    if (a == BossAttack::Leap) leapTarget_ = body_.position;  // wird beim Zielen nachgeführt
}

void Boss::chooseAttack(float dist) {
    struct Option {
        BossAttack attack;
        int weight;
    };
    std::vector<Option> options;
    const bool near = dist < def_->swingRange + def_->radius + 1.0f;
    if (phase_ >= 2 && summonTimer_ <= 0.0f) options.push_back({BossAttack::Summon, 8});
    if (near) {
        options.push_back({BossAttack::Swing, 6});
        options.push_back({BossAttack::Stomp, 2});
        if (phase_ >= 2) options.push_back({BossAttack::Spiral, 3});
        if (phase_ >= 2) options.push_back({BossAttack::Meteors, 2});
    } else {
        options.push_back({BossAttack::Stomp, 1});
        options.push_back({BossAttack::Volley, phase_ >= 2 ? 4 : 2});
        if (dist > 5.5f) options.push_back({BossAttack::Charge, 4});
        if (phase_ >= 2) options.push_back({BossAttack::Meteors, 4});
        if (phase_ >= 2) options.push_back({BossAttack::Spiral, 3});
        if (phase_ >= 3 && dist > 5.0f) options.push_back({BossAttack::Leap, 4});
    }
    // Wer nur aus der Ferne schießt, wird angerannt oder mit Meteoren belegt
    if (farTimer_ > 2.5f) {
        for (Option& o : options) {
            if (o.attack == BossAttack::Charge || o.attack == BossAttack::Leap || o.attack == BossAttack::Meteors) o.weight += 8;
        }
    }
    // Nicht zweimal hintereinander dasselbe
    for (Option& o : options) {
        if (o.attack == lastAttack_) o.weight = std::max(1, o.weight / 3);
    }
    int total = 0;
    for (const Option& o : options) total += o.weight;
    int pick = (int)(randomUnit() * (float)total);
    for (const Option& o : options) {
        pick -= o.weight;
        if (pick < 0) {
            beginTelegraph(o.attack);
            return;
        }
    }
    beginTelegraph(options.back().attack);
}

void Boss::enterPhase(int phase) {
    pendingPhase_ = phase;
    state_ = BossState::PhaseChange;
    attack_ = BossAttack::None;
    timer_ = def_->phaseChangeTime;
    body_.velocity.x = body_.velocity.z = 0.0f;
    flashTimer_ = 0.3f;
}

void Boss::finishTelegraph(Vector3 playerPos) {
    switch (attack_) {
        case BossAttack::Swing:
            state_ = BossState::Attack;
            timer_ = def_->swingActive;
            swingConsumed_ = false;
            body_.velocity.x = std::sin(yaw_) * def_->swingLunge;
            body_.velocity.z = std::cos(yaw_) * def_->swingLunge;
            break;
        case BossAttack::Stomp: {
            BossEvent e;
            e.position = body_.position;
            e.radius = def_->stompRadius;
            if (phase_ >= 3) {
                e.type = BossEvent::Type::Shockwave;
                e.damage = def_->waveDamage;
                e.speed = def_->waveSpeed;
                e.radius = def_->waveMaxRadius;
            } else {
                e.type = BossEvent::Type::Stomp;
                e.damage = def_->stompDamage;
            }
            events_.push_back(e);
            state_ = BossState::Recover;
            timer_ = def_->stompRecovery * phaseSpeed();
            break;
        }
        case BossAttack::Volley: {
            BossEvent e;
            e.type = BossEvent::Type::Volley;
            e.position = {body_.position.x, body_.position.y + def_->height * 0.55f, body_.position.z};
            e.direction = {std::sin(yaw_), 0.0f, std::cos(yaw_)};
            e.target = aimPoint_;   // Brusthöhe des Spielers: die Feuerbälle treffen auch, wenn man am Boden steht
            e.count = def_->volleyCount + (phase_ >= 3 ? 2 : 0);
            e.damage = def_->volleyDamage;
            e.speed = def_->volleySpeed;
            e.radius = def_->volleySpread;
            events_.push_back(e);
            state_ = BossState::Recover;
            timer_ = def_->volleyRecovery * phaseSpeed();
            break;
        }
        case BossAttack::Summon: {
            BossEvent e;
            e.type = BossEvent::Type::Summon;
            e.position = body_.position;
            e.count = def_->summonCount + (phase_ >= 3 ? 1 : 0);
            events_.push_back(e);
            summonTimer_ = def_->summonCooldown;
            state_ = BossState::Recover;
            timer_ = 0.8f;
            break;
        }
        case BossAttack::Leap:
            state_ = BossState::Attack;
            leapStart_ = body_.position;
            leapT_ = 0.0f;
            timer_ = def_->leapTime;
            (void)playerPos;
            break;
        case BossAttack::Charge: {
            state_ = BossState::Attack;
            timer_ = def_->chargeTime;
            chargeConsumed_ = false;
            chargeAge_ = 0.0f;
            chargeDir_ = {std::sin(yaw_), 0.0f, std::cos(yaw_)};
            break;
        }
        case BossAttack::Meteors: {
            BossEvent e;
            e.type = BossEvent::Type::Meteors;
            e.position = body_.position;
            e.target = playerPos;
            e.count = def_->meteorCount + (phase_ >= 3 ? 2 : 0);
            e.radius = def_->meteorRadius;
            e.damage = def_->meteorDamage;
            e.speed = def_->meteorDelay;
            events_.push_back(e);
            state_ = BossState::Recover;
            timer_ = 1.4f * phaseSpeed();
            break;
        }
        case BossAttack::Spiral:
            state_ = BossState::Attack;
            timer_ = def_->spiralDuration;
            spiralTick_ = 0.0f;
            spiralSign_ = randomUnit() < 0.5f ? 1.0f : -1.0f;
            spiralAngle_ = yaw_;
            break;
        case BossAttack::None:
            state_ = BossState::Chase;
            break;
    }
}

void Boss::update(float dt, const Level& level, Vector3 playerPos, bool playerAlive) {
    if (dt <= 0.0f) return;
    flashTimer_ = std::max(0.0f, flashTimer_ - dt);
    summonTimer_ = std::max(0.0f, summonTimer_ - dt);

    Vector3 pos = body_.position;
    Vector3 toPlayer{playerPos.x - pos.x, 0.0f, playerPos.z - pos.z};
    float dist = Vector3Length(toPlayer);
    const Vector3 preStep = body_.position;
    if (started() && state_ != BossState::Dead) farTimer_ = dist > 9.0f ? farTimer_ + dt : std::max(0.0f, farTimer_ - dt * 2.0f);
    Vector3 dir = dist > 1e-4f ? Vector3Scale(toPlayer, 1.0f / dist) : Vector3{std::sin(yaw_), 0.0f, std::cos(yaw_)};
    float yawToPlayer = std::atan2(dir.x, dir.z);

    bool useGravity = true;
    switch (state_) {
        case BossState::Waiting:
            brake(dt, 30.0f);
            if (playerAlive) yaw_ = turnToward(yaw_, yawToPlayer, 3.0f * dt);
            break;

        case BossState::Chase:
            if (!playerAlive) {
                brake(dt, 20.0f);
                break;
            }
            yaw_ = turnToward(yaw_, yawToPlayer, 5.0f * dt);
            body_.velocity.x = approach(body_.velocity.x, dir.x * def_->moveSpeed * (phase_ >= 3 ? 1.25f : 1.0f), 18.0f * dt);
            body_.velocity.z = approach(body_.velocity.z, dir.z * def_->moveSpeed * (phase_ >= 3 ? 1.25f : 1.0f), 18.0f * dt);
            decisionDelay_ -= dt;
            if (decisionDelay_ <= 0.0f) {
                decisionDelay_ = 0.35f;
                // Nur angreifen, wenn ein Angriff Sinn ergibt (nah genug oder ein Fernangriff bereit ist)
                if (dist < def_->swingRange + def_->radius + 0.6f || dist > 5.0f) chooseAttack(dist);
            }
            break;

        case BossState::Telegraph:
            brake(dt, 40.0f);
            timer_ -= dt;
            // Zielen bis kurz vor dem Angriff, dann festgelegt (ausweichen ist möglich)
            if (timer_ > telegraphTotal_ * 0.3f && playerAlive) {
                yaw_ = turnToward(yaw_, yawToPlayer, 8.0f * dt);
                if (attack_ == BossAttack::Leap) leapTarget_ = {playerPos.x, playerPos.y, playerPos.z};
                aimPoint_ = {playerPos.x, playerPos.y + 0.9f, playerPos.z};
            }
            if (timer_ <= 0.0f) finishTelegraph(playerPos);
            break;

        case BossState::Attack:
            if (attack_ == BossAttack::Leap) {
                // Sprung wird direkt entlang einer Parabel geführt
                useGravity = false;
                leapT_ = std::min(1.0f, leapT_ + dt / std::max(0.1f, def_->leapTime));
                Vector3 flat = Vector3Lerp(leapStart_, leapTarget_, leapT_);
                float arc = 4.0f * def_->leapHeight * leapT_ * (1.0f - leapT_);
                Vector3 next{flat.x, flat.y + arc, flat.z};
                body_.velocity = {(next.x - body_.position.x) / dt, 0.0f, (next.z - body_.position.z) / dt};
                body_.position = next;
                body_.grounded = false;
                if (leapT_ >= 1.0f) {
                    BossEvent e;
                    e.type = BossEvent::Type::LeapLand;
                    e.position = body_.position;
                    e.radius = def_->leapRadius;
                    e.damage = def_->leapDamage;
                    events_.push_back(e);
                    body_.velocity = {0, 0, 0};
                    state_ = BossState::Exhausted;
                    timer_ = def_->exhaustedTime;
                    attack_ = BossAttack::None;
                }
            } else if (attack_ == BossAttack::Charge) {
                timer_ -= dt;
                chargeAge_ += dt;
                yaw_ = std::atan2(chargeDir_.x, chargeDir_.z);
                body_.velocity.x = chargeDir_.x * def_->chargeSpeed;
                body_.velocity.z = chargeDir_.z * def_->chargeSpeed;
                if (timer_ <= 0.0f) {
                    state_ = BossState::Recover;
                    timer_ = 0.9f * phaseSpeed();
                }
            } else if (attack_ == BossAttack::Spiral) {
                brake(dt, 40.0f);
                timer_ -= dt;
                spiralAngle_ += spiralSign_ * def_->spiralTurn * dt;
                yaw_ = spiralAngle_;
                spiralTick_ -= dt;
                if (spiralTick_ <= 0.0f) {
                    spiralTick_ = def_->spiralInterval / (phase_ >= 3 ? 1.25f : 1.0f);
                    int arms = std::max(1, def_->spiralArms + (phase_ >= 3 ? 1 : 0));
                    for (int a = 0; a < arms; a++) {
                        float ang = spiralAngle_ + 6.2831853f * (float)a / (float)arms;
                        BossEvent e;
                        e.type = BossEvent::Type::SpiralShot;
                        e.position = body_.position;
                        e.direction = {std::sin(ang), 0.0f, std::cos(ang)};
                        e.damage = def_->spiralDamage;
                        e.speed = def_->spiralSpeed;
                        events_.push_back(e);
                    }
                }
                if (timer_ <= 0.0f) {
                    state_ = BossState::Recover;
                    timer_ = 1.1f * phaseSpeed();
                }
            } else {
                timer_ -= dt;
                if (timer_ <= 0.0f) {
                    state_ = BossState::Recover;
                    timer_ = def_->swingRecovery * phaseSpeed();
                }
            }
            break;

        case BossState::Recover:
            brake(dt, 30.0f);
            timer_ -= dt;
            if (timer_ <= 0.0f) {
                // Schwere Angriffe machen müde
                bool heavy = attack_ == BossAttack::Stomp && phase_ >= 3;
                attack_ = BossAttack::None;
                if (heavy) {
                    state_ = BossState::Exhausted;
                    timer_ = def_->exhaustedTime * 0.6f;
                } else {
                    state_ = BossState::Chase;
                    decisionDelay_ = 0.4f;
                }
            }
            break;

        case BossState::Exhausted:
            brake(dt, 30.0f);
            timer_ -= dt;
            if (timer_ <= 0.0f) {
                state_ = BossState::Chase;
                decisionDelay_ = 0.5f;
            }
            break;

        case BossState::PhaseChange:
            brake(dt, 40.0f);
            timer_ -= dt;
            if (timer_ <= 0.0f) {
                phase_ = pendingPhase_;
                BossEvent e;
                e.type = BossEvent::Type::PhaseChange;
                e.phase = phase_;
                e.position = body_.position;
                events_.push_back(e);
                if (phase_ >= 2) summonTimer_ = 0.0f;  // die Rufer kommen gleich zu Beginn der neuen Phase
                state_ = BossState::Chase;
                decisionDelay_ = 0.5f;
            }
            break;

        case BossState::Dead:
            brake(dt, 10.0f);
            deadTimer_ += dt;
            break;
    }

    body_.step(dt, level, useGravity ? kGravity : 0.0f, kMaxFall);
    if (state_ == BossState::Attack && attack_ == BossAttack::Charge && chargeAge_ > 0.2f) {
        float moved = std::hypot(body_.position.x - preStep.x, body_.position.z - preStep.z);
        if (moved < def_->chargeSpeed * dt * 0.35f) {
            // Gegen die Wand gerannt: benommen und verwundbar
            BossEvent e;
            e.type = BossEvent::Type::ChargeCrash;
            e.position = body_.position;
            e.radius = 4.0f;
            events_.push_back(e);
            body_.velocity = {0, 0, 0};
            state_ = BossState::Exhausted;
            timer_ = def_->exhaustedTime * 1.3f;
            attack_ = BossAttack::None;
        }
    }
    if (!useGravity) {
        // Während des Sprungs bestimmt die Parabel die Höhe; Wände und Levelgrenzen gelten trotzdem
        level.resolveHorizontal(body_.position, body_.radius, body_.height);
    }
}

float Boss::takeHit(float damage, Vector3 knockback) {
    if (!targetable()) return 0.0f;
    float mult = state_ == BossState::Exhausted ? def_->exhaustedDamageMult : 1.0f;
    float dealt = health_.damage(damage * mult);
    flashTimer_ = 0.12f;
    (void)knockback;  // Bosse werden nicht zurückgestoßen

    if (health_.dead()) {
        state_ = BossState::Dead;
        attack_ = BossAttack::None;
        deadTimer_ = 0.0f;
        body_.velocity = {0, 0, 0};
        return dealt;
    }
    float f = health_.fraction();
    if (phase_ == 1 && pendingPhase_ < 2 && f <= def_->phase2At) enterPhase(2);
    else if (phase_ <= 2 && pendingPhase_ < 3 && f <= def_->phase3At) enterPhase(3);
    return dealt;
}

// ---------------------------------------------------------------- Zeichnen

void Boss::draw(const LitRenderer& r, const Level& level) const {
    const Vector3 p = body_.position;
    const float rad = def_->radius, h = def_->height;
    float shrink = state_ == BossState::Dead ? std::max(0.0f, 1.0f - deadTimer_ / kDeathDuration) : 1.0f;
    if (shrink <= 0.0f) return;

    float ground = level.groundHeight(p, rad, p.y);
    if (ground > -1000.0f) {
        float airborne = std::max(0.0f, p.y - ground);
        r.cylinder({p.x, ground + 0.02f, p.z}, rad * 1.25f * shrink / (1.0f + airborne * 0.15f), 0.01f, Color{0, 0, 0, 100});
    }

    // Warnflächen am Boden
    if (state_ == BossState::Telegraph) {
        float k = telegraphProgress();
        float pulse = 0.5f + 0.5f * std::sin((float)GetTime() * 20.0f);
        if (attack_ == BossAttack::Stomp && phase_ < 3) {
            r.cylinder({p.x, ground + 0.03f, p.z}, def_->stompRadius, 0.01f, Color{255, 60, 40, (unsigned char)(40 + 60 * k)});
            r.cylinder({p.x, ground + 0.035f, p.z}, def_->stompRadius * k, 0.01f, Color{255, 190, 60, (unsigned char)(80 + 80 * pulse)});
        } else if (attack_ == BossAttack::Stomp) {
            r.cylinder({p.x, ground + 0.03f, p.z}, 2.5f + 2.5f * k, 0.01f, Color{255, 200, 80, (unsigned char)(60 + 90 * pulse)});
        } else if (attack_ == BossAttack::Charge) {
            // Bahn des Ansturms: schmaler roter Streifen vom Boss nach vorn, füllt sich bis zum Start
            float len = chargeTelegraphLen();
            Vector3 dirC{std::sin(yaw_), 0.0f, std::cos(yaw_)};
            Vector3 mid{p.x + dirC.x * len * 0.5f, ground + 0.03f, p.z + dirC.z * len * 0.5f};
            r.box(mid, {rad * 1.8f, 0.02f, len}, Color{255, 60, 40, (unsigned char)(35 + 50 * k)}, yaw_);
            float fill = len * k;
            Vector3 mid2{p.x + dirC.x * fill * 0.5f, ground + 0.035f, p.z + dirC.z * fill * 0.5f};
            r.box(mid2, {rad * 1.8f, 0.02f, fill}, Color{255, 190, 60, (unsigned char)(70 + 90 * pulse)}, yaw_);
        } else if (attack_ == BossAttack::Spiral || attack_ == BossAttack::Meteors) {
            r.cylinder({p.x, ground + 0.03f, p.z}, 2.2f + 1.2f * k, 0.01f, Color{255, 200, 80, (unsigned char)(50 + 90 * pulse)});
        } else if (attack_ == BossAttack::Leap) {
            float lg = level.groundHeight(leapTarget_, 0.3f, leapTarget_.y + 0.5f);
            r.cylinder({leapTarget_.x, lg + 0.03f, leapTarget_.z}, def_->leapRadius, 0.01f, Color{255, 60, 40, (unsigned char)(50 + 70 * k)});
            r.cylinder({leapTarget_.x, lg + 0.035f, leapTarget_.z}, def_->leapRadius * k, 0.01f, Color{255, 200, 80, (unsigned char)(90 + 90 * pulse)});
        }
    }

    Vector3 fwd{std::sin(yaw_), 0.0f, std::cos(yaw_)};
    Vector3 side{-fwd.z, 0.0f, fwd.x};

    float progress = telegraphProgress();
    Color body = def_->color;
    if (state_ == BossState::Exhausted) body = lerpColor(def_->color, Color{110, 110, 150, 255}, 0.55f);
    if (phase_ >= 3) body = lerpColor(body, Color{220, 60, 40, 255}, 0.25f);
    if (flashTimer_ > 0.0f) {
        body = WHITE;
    } else if (state_ == BossState::Telegraph) {
        float pulse = 0.5f + 0.5f * std::sin((float)GetTime() * 26.0f);
        body = lerpColor(body, Color{255, 226, 90, 255}, 0.3f + 0.6f * pulse * progress);
    } else if (state_ == BossState::PhaseChange) {
        float pulse = 0.5f + 0.5f * std::sin((float)GetTime() * 18.0f);
        body = lerpColor(body, Color{255, 255, 255, 255}, 0.3f + 0.4f * pulse);
    }

    float lean = state_ == BossState::Telegraph ? -0.5f * progress : (state_ == BossState::Attack && attack_ == BossAttack::Swing ? 0.6f : 0.0f);
    float squat = (state_ == BossState::Telegraph && (attack_ == BossAttack::Stomp || attack_ == BossAttack::Leap)) ? 1.0f - 0.25f * progress : 1.0f;
    Vector3 c{p.x + fwd.x * lean, p.y + h * 0.5f * shrink * squat, p.z + fwd.z * lean};

    const bool golem = def_->style == "golem";
    const bool storm = def_->style == "storm";
    if (storm) c.y += 0.45f + 0.12f * std::sin((float)GetTime() * 2.2f);  // der Sturmwächter schwebt
    const std::string skinName = golem ? "golem_skin" : (storm ? "storm_body" : "king_skin");
    auto skin = [&]() { r.useMaterial(flashTimer_ > 0.0f ? kNoMaterial : r.material(skinName)); };
    r.setAnchor(p, yaw_);

    // Körper, Bauch (beim Golem ein glühender Kern), Kopf
    skin();
    r.ellipsoid(c, {rad * shrink, h * 0.42f * shrink * squat, rad * shrink}, body);
    Vector3 belly{c.x + fwd.x * rad * 0.35f, c.y - h * 0.06f * squat, c.z + fwd.z * rad * 0.35f};
    if (golem || storm) {
        float pulse = 0.5f + 0.5f * std::sin((float)GetTime() * (phase_ >= 3 ? 9.0f : 4.0f));
        Color hot = storm ? Color{120, 200, 255, 255} : Color{255, 120, 30, 255};
        Color hotter = storm ? Color{230, 250, 255, 255} : Color{255, 220, 110, 255};
        Color core = lerpColor(hot, hotter, 0.3f + 0.5f * pulse);
        core.a = 244;
        r.clearMaterial();
        r.sphere(belly, rad * 0.36f * shrink, core);
        r.sphere(belly, rad * (0.52f + 0.06f * pulse) * shrink, storm ? Color{140, 210, 255, 70} : Color{255, 140, 40, 70});
    } else {
        r.ellipsoid(belly, {rad * 0.7f * shrink, h * 0.3f * shrink * squat, rad * 0.7f * shrink}, lerpColor(body, Color{240, 210, 150, 255}, 0.4f));
    }
    Vector3 head{c.x + fwd.x * rad * 0.3f, c.y + h * 0.42f * squat, c.z + fwd.z * rad * 0.3f};
    skin();
    r.ellipsoid(head, {rad * 0.62f * shrink, rad * 0.55f * shrink, rad * 0.62f * shrink}, body);
    r.clearMaterial();

    if (storm) {
        // Heiligenschein und kreisende Windkugeln statt Krone
        r.cylinder({head.x, head.y + rad * 0.62f * shrink, head.z}, rad * 0.55f * shrink, rad * 0.05f * shrink, Color{220, 245, 255, 235});
        float spin = (float)GetTime() * (phase_ >= 3 ? 3.6f : 1.6f);
        for (int i = 0; i < 4; i++) {
            float a = spin + 1.5708f * (float)i;
            r.sphere({c.x + std::cos(a) * rad * 1.55f * shrink, c.y + std::sin(a * 1.7f) * h * 0.12f, c.z + std::sin(a) * rad * 1.55f * shrink},
                     rad * 0.17f * shrink, Color{190, 235, 255, 240});
        }
    } else if (golem) {
        // Steinerne Schulterplatten statt Krone
        for (float s : {-1.0f, 1.0f}) {
            r.ellipsoid({c.x + side.x * s * rad * 0.85f, c.y + h * 0.3f * squat, c.z + side.z * s * rad * 0.85f},
                        {rad * 0.42f * shrink, rad * 0.3f * shrink, rad * 0.42f * shrink}, lerpColor(body, Color{20, 18, 20, 255}, 0.3f));
        }
    } else {
        Color gold{240, 200, 80, 255};
        r.useMaterial(flashTimer_ > 0.0f ? kNoMaterial : r.material("gold"));
        r.cylinder({head.x, head.y + rad * 0.45f * shrink, head.z}, rad * 0.58f * shrink, rad * 0.16f * shrink, gold);
        for (int i = 0; i < 5; i++) {
            float a = 6.2831853f * (float)i / 5.0f;
            r.cylinder({head.x + std::cos(a) * rad * 0.48f * shrink, head.y + rad * 0.6f * shrink, head.z + std::sin(a) * rad * 0.48f * shrink},
                       rad * 0.08f * shrink, rad * 0.4f * shrink, gold);
        }
        r.clearMaterial();
    }
    // Augen
    Color eye = storm ? Color{120, 240, 255, 255} : golem ? Color{255, 150, 40, 255} : (phase_ >= 3 ? Color{255, 80, 60, 255} : Color{255, 236, 110, 255});
    for (float s : {-1.0f, 1.0f}) {
        r.sphere({head.x + fwd.x * rad * 0.5f * shrink + side.x * s * rad * 0.24f * shrink, head.y + rad * 0.08f * shrink,
                  head.z + fwd.z * rad * 0.5f * shrink + side.z * s * rad * 0.24f * shrink},
                 rad * 0.11f * shrink, eye);
    }

    // Arme: heben sich bei der Vorwarnung, schlagen beim Hieb nach vorn
    float armUp = state_ == BossState::Telegraph ? progress * 1.3f : 0.0f;
    float armFwd = (state_ == BossState::Attack && attack_ == BossAttack::Swing) ? 1.3f : 0.3f;
    for (float s : {-1.0f, 1.0f}) {
        Vector3 hand{c.x + side.x * s * rad * 1.05f * shrink + fwd.x * rad * armFwd * shrink,
                     c.y - h * 0.05f + armUp * h * 0.3f,
                     c.z + side.z * s * rad * 1.05f * shrink + fwd.z * rad * armFwd * shrink};
        skin();
        r.ellipsoid(hand, {rad * 0.36f * shrink, rad * 0.36f * shrink, rad * 0.36f * shrink}, lerpColor(body, Color{50, 30, 30, 255}, 0.25f));
        r.clearMaterial();
    }
    // Beine (der Sturmwächter hat stattdessen einen auslaufenden Schweif)
    if (storm) {
        skin();
        r.ellipsoid({p.x - fwd.x * rad * 0.2f, c.y - h * 0.55f, p.z - fwd.z * rad * 0.2f}, {rad * 0.5f * shrink, h * 0.2f * shrink, rad * 0.5f * shrink}, lerpColor(body, WHITE, 0.3f));
        r.ellipsoid({p.x - fwd.x * rad * 0.35f, c.y - h * 0.85f, p.z - fwd.z * rad * 0.35f}, {rad * 0.28f * shrink, h * 0.16f * shrink, rad * 0.28f * shrink}, lerpColor(body, WHITE, 0.5f));
        r.clearMaterial();
        r.clearAnchor();
        return;
    }
    skin();
    for (float s : {-1.0f, 1.0f}) {
        r.cylinder({p.x + side.x * s * rad * 0.45f, p.y, p.z + side.z * s * rad * 0.45f}, rad * 0.3f * shrink, h * 0.22f * shrink,
                   lerpColor(body, Color{50, 30, 30, 255}, 0.35f));
    }
    r.clearMaterial();
    r.clearAnchor();
}

}  // namespace aldoria
