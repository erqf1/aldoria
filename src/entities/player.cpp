#include "entities/player.h"

#include "entities/rig.h"

#include <algorithm>
#include <cmath>

#include "core/events.h"
#include "core/file_util.h"
#include "core/log.h"
#include "core/math_util.h"
#include "raymath.h"

namespace aldoria {

PlayerConfig PlayerConfig::fromJson(const json& j) {
    PlayerConfig c;
    if (!j.is_object()) return c;
    c.walkSpeed = j.value("walk_speed", c.walkSpeed);
    c.sprintSpeed = j.value("sprint_speed", c.sprintSpeed);
    c.groundAccel = j.value("ground_accel", c.groundAccel);
    c.groundDecel = j.value("ground_decel", c.groundDecel);
    c.airAccel = j.value("air_accel", c.airAccel);
    c.turnRate = j.value("turn_rate", c.turnRate);
    c.radius = j.value("radius", c.radius);
    c.height = j.value("height", c.height);
    c.jumpSpeed = j.value("jump_speed", c.jumpSpeed);
    c.gravity = j.value("gravity", c.gravity);
    c.maxFallSpeed = j.value("max_fall_speed", c.maxFallSpeed);
    c.coyoteTime = j.value("coyote_time", c.coyoteTime);
    c.jumpBuffer = j.value("jump_buffer", c.jumpBuffer);
    c.jumpCut = j.value("jump_cut", c.jumpCut);
    c.maxHealth = j.value("max_health", c.maxHealth);
    c.attackBuffer = j.value("attack_buffer", c.attackBuffer);
    c.dodgeBuffer = j.value("dodge_buffer", c.dodgeBuffer);
    c.dodgeSpeed = j.value("dodge_speed", c.dodgeSpeed);
    c.dodgeDuration = j.value("dodge_duration", c.dodgeDuration);
    c.dodgeIFrames = j.value("dodge_iframes", c.dodgeIFrames);
    c.dodgeCooldown = j.value("dodge_cooldown", c.dodgeCooldown);
    c.hurtTime = j.value("hurt_time", c.hurtTime);
    c.hurtInvulnerability = j.value("hurt_invulnerability", c.hurtInvulnerability);
    c.dashSpeed = j.value("dash_speed", c.dashSpeed);
    c.dashDuration = j.value("dash_duration", c.dashDuration);
    c.dashCooldown = j.value("dash_cooldown", c.dashCooldown);
    c.dashBuffer = j.value("dash_buffer", c.dashBuffer);
    c.dashIFrames = j.value("dash_iframes", c.dashIFrames);
    c.sparkCooldown = j.value("spark_cooldown", c.sparkCooldown);
    c.sparkDamage = j.value("spark_damage", c.sparkDamage);
    c.sparkSpeed = j.value("spark_speed", c.sparkSpeed);
    c.flaskHeal = j.value("flask_heal", c.flaskHeal);
    return c;
}

PlayerConfig PlayerConfig::loadFromFile(const std::string& path) {
    std::string error;
    auto j = loadJsonFile(path, error);
    if (!j) {
        Log::warn(LogCategory::Loading, "Spieler-Konfiguration: {}. Nutze Vorgaben", error);
        return {};
    }
    return fromJson(*j);
}

const char* playerStateName(PlayerState s) {
    switch (s) {
        case PlayerState::Normal: return "normal";
        case PlayerState::Attacking: return "Angriff";
        case PlayerState::Dodging: return "Ausweichen";
        case PlayerState::Dashing: return "Dash";
        case PlayerState::Hurt: return "getroffen";
        case PlayerState::Dead: return "tot";
    }
    return "?";
}

// ---------------------------------------------------------------- Aufbau

void Player::refreshMaxHealth() {
    float before = health_.max();
    float target = cfg_.maxHealth + mods_.maxHealthAdd + bonusMaxHealth_;
    health_.setMax(target, /*refill=*/false);
    if (target > before && !health_.dead()) health_.heal(target - before);  // mehr Maximum heilt um die Differenz
    health_.resistance = mods_.resistance;
}

void Player::configure(const PlayerConfig& config) {
    cfg_ = config;
    body_.radius = cfg_.radius;
    body_.height = cfg_.height;
    refreshMaxHealth();
}

void Player::setModifiers(const PlayerModifiers& mods) {
    mods_ = mods;
    refreshMaxHealth();
}

void Player::setBonusMaxHealth(float bonus) {
    bonusMaxHealth_ = bonus;
    refreshMaxHealth();
}

void Player::setWeapon(WeaponDef weapon) {
    weapon_ = std::move(weapon);
    comboStep_ = 0;
    comboTimer_ = 0.0f;
}

void Player::spawn(Vector3 position) {
    spawnPoint_ = position;
    body_.position = position;
    body_.velocity = {0, 0, 0};
    body_.grounded = false;
    health_.refill();
    state_ = PlayerState::Normal;
    stateTimer_ = 0.0f;
    coyote_ = jumpBuffer_ = attackBuffer_ = dodgeBuffer_ = dodgeCooldown_ = comboTimer_ = 0.0f;
    dashBuffer_ = dashCooldown_ = castBuffer_ = sparkCooldown_ = castGlow_ = 0.0f;
    comboStep_ = 0;
    jumping_ = false;
    airDashUsed_ = false;
    airJumpUsed_ = false;
    castRequested_ = false;
    capeInit_ = false;
    pose_ = 1.0f;
}

void Player::teleport(Vector3 position, float yaw) {
    body_.position = position;
    body_.velocity = {0, 0, 0};
    body_.grounded = false;
    yaw_ = yaw;
    if (state_ != PlayerState::Dead) state_ = PlayerState::Normal;
    stateTimer_ = 0.0f;
    attackBuffer_ = jumpBuffer_ = dodgeBuffer_ = dashBuffer_ = castBuffer_ = 0.0f;
    comboStep_ = 0;
    comboTimer_ = 0.0f;
    castRequested_ = false;
    spawnPoint_ = position;
    capeInit_ = false;
}

Player::AttackTimes Player::attackTimes(const AttackDef& a) const {
    float k = 1.0f / std::max(0.2f, mods_.attackSpeedMult);
    return {a.windup * k, a.active * k, a.recovery * k};
}

MeleeQuery Player::attackQuery() const {
    const AttackDef& a = currentAttack();
    return {body_.position, yaw_, a.range * mods_.rangeMult, a.halfAngle};
}

bool Player::canBeHit() const {
    if (godMode || state_ == PlayerState::Dead || health_.dead()) return false;
    if (health_.invulnerable()) return false;
    if (state_ == PlayerState::Dodging && stateTimer_ < cfg_.dodgeIFrames + mods_.dodgeIFramesAdd) return false;
    if (state_ == PlayerState::Dashing && stateTimer_ < cfg_.dashIFrames) return false;
    return true;
}

float Player::takeDamage(float amount, Vector3 knockbackVelocity, EventBus& events) {
    if (!canBeHit()) return 0.0f;
    float dealt = health_.damage(amount);
    if (dealt <= 0.0f) return 0.0f;

    body_.velocity.x = knockbackVelocity.x;
    body_.velocity.z = knockbackVelocity.z;
    comboStep_ = 0;
    comboTimer_ = attackBuffer_ = dodgeBuffer_ = dashBuffer_ = castBuffer_ = 0.0f;
    stateTimer_ = 0.0f;
    events.emit(PlayerDamaged{dealt, health_.current()});

    if (health_.dead()) {
        state_ = PlayerState::Dead;
        Log::info(LogCategory::Combat, "Spieler besiegt");
        events.emit(PlayerDied{});
    } else {
        health_.grantInvulnerability(cfg_.hurtInvulnerability);
        state_ = PlayerState::Hurt;
    }
    return dealt;
}

float Player::healBy(float amount, EventBus& events) {
    float healed = health_.heal(amount);
    if (healed > 0.0f) events.emit(PlayerHealed{healed});
    return healed;
}

bool Player::consumeCast(Vector3& origin, Vector3& direction) {
    if (!castRequested_) return false;
    castRequested_ = false;
    origin = castOrigin_;
    direction = castDirection_;
    return true;
}

float Player::sparkReadyFraction() const {
    float total = std::max(0.1f, cfg_.sparkCooldown * mods_.sparkCooldownMult);
    return std::clamp(1.0f - sparkCooldown_ / total, 0.0f, 1.0f);
}

float Player::dashReadyFraction() const {
    return std::clamp(1.0f - dashCooldown_ / std::max(0.1f, cfg_.dashCooldown), 0.0f, 1.0f);
}

// ---------------------------------------------------------------- Update

void Player::update(float dt, const PlayerInput& in, const PlayerFrame& frame, EventBus& events) {
    // Eingaben vormerken, auch wenn die Spielzeit steht (Hit-Stop), damit nichts verschluckt wird
    if (in.attackPressed) attackBuffer_ = cfg_.attackBuffer;
    if (in.jumpPressed) jumpBuffer_ = cfg_.jumpBuffer;
    if (in.dodgePressed) dodgeBuffer_ = cfg_.dodgeBuffer;
    if (in.dashPressed) dashBuffer_ = cfg_.dashBuffer;
    if (in.secondaryPressed) castBuffer_ = cfg_.attackBuffer;
    if (dt <= 0.0f || !frame.level) return;
    const Level& level = *frame.level;

    health_.update(dt);
    attackBuffer_ = std::max(0.0f, attackBuffer_ - dt);
    jumpBuffer_ = std::max(0.0f, jumpBuffer_ - dt);
    dodgeBuffer_ = std::max(0.0f, dodgeBuffer_ - dt);
    dashBuffer_ = std::max(0.0f, dashBuffer_ - dt);
    castBuffer_ = std::max(0.0f, castBuffer_ - dt);
    dodgeCooldown_ = std::max(0.0f, dodgeCooldown_ - dt);
    dashCooldown_ = std::max(0.0f, dashCooldown_ - dt);
    sparkCooldown_ = std::max(0.0f, sparkCooldown_ - dt);
    castGlow_ = std::max(0.0f, castGlow_ - dt);
    comboTimer_ = std::max(0.0f, comboTimer_ - dt);

    // Kamerarelative Wunschrichtung
    Vector3 forward{std::sin(frame.cameraYaw), 0.0f, std::cos(frame.cameraYaw)};
    Vector3 right{-forward.z, 0.0f, forward.x};
    Vector3 wish = Vector3Add(Vector3Scale(forward, in.move.y), Vector3Scale(right, in.move.x));

    switch (state_) {
        case PlayerState::Normal: updateNormal(dt, in, frame, wish, events); break;
        case PlayerState::Attacking: updateAttacking(dt, in, frame, wish, events); break;
        case PlayerState::Dodging: updateDodging(dt); break;
        case PlayerState::Dashing: updateDashing(dt); break;
        case PlayerState::Hurt: updateHurt(dt); break;
        case PlayerState::Dead:
            body_.velocity.x = approach(body_.velocity.x, 0.0f, 25.0f * dt);
            body_.velocity.z = approach(body_.velocity.z, 0.0f, 25.0f * dt);
            break;
    }

    // Beim Dash schwebt man kurz geradeaus (keine Schwerkraft)
    float gravity = state_ == PlayerState::Dashing ? 0.0f : cfg_.gravity;
    float impact = body_.step(dt, level, gravity, cfg_.maxFallSpeed);
    if (impact > 4.0f) events.emit(PlayerLanded{impact});
    if (impact > 4.0f) landDip_ = std::min(0.2f, 0.06f + impact * 0.008f);
    landDip_ = std::max(0.0f, landDip_ - dt * 0.9f);
    pose_ = approach(pose_, state_ == PlayerState::Dodging ? 0.66f : 1.0f, dt * 8.0f);
    deadAnim_ = dead() ? deadAnim_ + dt : 0.0f;
    if (body_.grounded) {
        jumping_ = false;
        airDashUsed_ = false;
        airJumpUsed_ = false;
        animPhase_ += dt * std::hypot(body_.velocity.x, body_.velocity.z) * 1.7f;  // Schrittfolge wächst mit dem Tempo
    }

    simulateCape(dt);

    if (body_.position.y < level.groundY - 30.0f) {
        Log::warn(LogCategory::Player, "Spieler ist aus der Welt gefallen, setze zurück");
        spawn(spawnPoint_);
        events.emit(PlayerRespawned{});
    }
}

namespace {

float easeOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

Vector3 dirFromAngle(float a) { return {std::sin(a), 0.0f, std::cos(a)}; }

float smooth01(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Schiebt einen Punkt auf dem kürzesten Weg aus einer Kapsel (Strecke a-b, Radius R) heraus. Kein Spiegeln auf die andere Seite:
// das ließe den Stoff bei schnellen Drehungen von einem Bild zum nächsten springen. Der Stoff wird stattdessen um den Körper geschoben.
void pushOutOfCapsule(Vector3& p, Vector3 a, Vector3 b, float radius, Vector3 fwd, bool back) {
    Vector3 ab = Vector3Subtract(b, a);
    float len2 = Vector3DotProduct(ab, ab);
    float t = len2 > 1e-6f ? std::clamp(Vector3DotProduct(Vector3Subtract(p, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
    Vector3 closest = Vector3Add(a, Vector3Scale(ab, t));
    Vector3 d = Vector3Subtract(p, closest);
    float dist = Vector3Length(d);
    if (dist >= radius) return;
    Vector3 n = dist > 1e-4f ? Vector3Scale(d, 1.0f / dist) : Vector3Scale(fwd, -1.0f);
    if (back && dist < 0.02f) n = Vector3Scale(fwd, -1.0f);   // genau auf der Achse: nach hinten
    p = Vector3Add(closest, Vector3Scale(n, radius));
}

}  // namespace

// Gemeinsame Haltung des Körpers für Zeichnen und Umhang
Player::RigFrame Player::rigFrame() const {
    RigFrame f;
    const bool dodging = state_ == PlayerState::Dodging;
    f.fwd = dirFromAngle(yaw_);
    f.side = {-f.fwd.z, 0.0f, f.fwd.x};
    f.pose = pose_;
    const float speed = std::hypot(body_.velocity.x, body_.velocity.z);
    f.moveK = body_.grounded ? std::min(1.0f, speed / 6.0f) : 0.0f;
    f.hipY = 0.88f * f.pose - landDip_ + std::fabs(std::sin(animPhase_)) * 0.035f * f.moveK;
    f.lean = leanAngle();
    f.tumble = dodging ? 6.2831853f * std::clamp(stateTimer_ / std::max(0.05f, cfg_.dodgeDuration), 0.0f, 1.0f) : 0.0f;
    f.fallA = dead() ? 1.45f * easeOut(std::clamp(deadAnim_ / 0.4f, 0.0f, 1.0f)) : 0.0f;
    return f;
}

// Punkt aus Körperkoordinaten (x rechts, y oben, z vorn) in die Welt. `upper`: neigt sich mit dem Oberkörper.
Vector3 Player::rigPoint(const RigFrame& f, Vector3 at, Vector3 local, bool upper) const {
    Vector3 v = local;
    if (upper) v = pitchAround(v, {0, f.hipY, 0}, f.lean);
    if (f.tumble > 0.0f) v = pitchAround(v, {0, 0.45f, 0}, f.tumble);
    if (f.fallA > 0.0f) v = rollAround(v, {0, 0, 0}, f.fallA);
    return {at.x + f.side.x * v.x + f.fwd.x * v.z, at.y + v.y, at.z + f.side.z * v.x + f.fwd.z * v.z};
}

// Wohin der Fuß der Seite `s` (+1 rechts, -1 links) zeigt, in Körperkoordinaten
Vector3 Player::footTarget(const RigFrame& f, float s) const {
    const bool dodging = state_ == PlayerState::Dodging;
    const bool inAir = !body_.grounded && state_ != PlayerState::Dashing;
    const float phi = animPhase_ + (s > 0.0f ? 0.0f : kPi);
    if (dodging) return {s * 0.12f, f.hipY - 0.34f, 0.10f + 0.10f * s};
    if (inAir) {
        float k = std::clamp(body_.velocity.y / 9.0f, -1.0f, 1.0f);   // > 0 steigt (Knie hoch), < 0 fällt (Beine strecken)
        return {s * 0.13f, f.hipY - 0.58f - 0.30f * (1.0f - k) * 0.5f + (s > 0.0f ? 0.05f : -0.03f), (s > 0.0f ? 0.16f : -0.10f) * (0.5f + 0.5f * k)};
    }
    Vector3 foot{s * 0.13f, 0.07f + std::max(0.0f, std::cos(phi)) * 0.20f * f.moveK, std::sin(phi) * 0.34f * f.moveK};
    if (state_ == PlayerState::Attacking) foot.z += s * (phase_ == AttackPhase::Active ? 0.30f : 0.16f);
    return foot;
}

// Ruheform des Umhangs in Körperkoordinaten: ein weiter Mantel, der von den Schultern über den Rücken fällt,
// unten breiter wird und an den Seiten leicht nach vorn um die Schultern greift
Vector3 Player::capeRestLocal(const RigFrame& f, int c, int r) const {
    const float t = (float)c / (float)(kClothCols - 1) - 0.5f;    // -0.5 links ... 0.5 rechts
    const float k = (float)r / (float)(kClothRows - 1);           // 0 Schulter ... 1 Saum
    const float width = 0.60f + 0.46f * k;
    const float length = 0.98f * f.pose;
    const float wrap = 0.10f * (2.0f * t) * (2.0f * t) * (1.0f - 0.55f * k);
    return {t * width, f.hipY + 0.49f * f.pose - length * k, -(0.265f + 0.07f * k) + wrap};
}

float Player::leanAngle() const {
    if (dead()) return 0.0f;
    float speed = std::hypot(body_.velocity.x, body_.velocity.z);
    // Aufrechte Haltung: nur ein Hauch Vorwärtsneigung im Lauf
    float lean = std::min(speed, 8.5f) * (body_.grounded ? 0.007f : 0.004f);
    if (state_ == PlayerState::Dashing) return 0.42f;
    if (state_ == PlayerState::Attacking) {
        switch (phase_) {
            case AttackPhase::Windup: return -0.08f;
            case AttackPhase::Active: return 0.24f;
            case AttackPhase::Recovery: return 0.08f;
        }
    }
    if (state_ == PlayerState::Hurt) return -0.32f * std::clamp(1.0f - stateTimer_ / std::max(0.1f, cfg_.hurtTime), 0.0f, 1.0f);
    return lean;
}

Player::CapeColliders Player::capeColliders(const RigFrame& f, Vector3 at) const {
    CapeColliders c;
    const float hip = f.hipY;
    c.torsoA = rigPoint(f, at, {0, hip - 0.02f, 0}, false);
    c.torsoB = rigPoint(f, at, {0, hip + 0.40f, 0}, true);
    c.skirtA = rigPoint(f, at, {0, hip - 0.25f, 0}, false);
    c.skirtB = rigPoint(f, at, {0, hip + 0.02f, 0}, false);
    c.shoulderL = rigPoint(f, at, {-0.34f, hip + 0.44f * f.pose, 0}, true);
    c.shoulderR = rigPoint(f, at, {0.34f, hip + 0.44f * f.pose, 0}, true);
    for (int s = 0; s < 2; s++) {
        float sd = s == 0 ? -1.0f : 1.0f;
        c.legTop[s] = rigPoint(f, at, {sd * 0.13f, hip - 0.03f, 0}, false);
        c.legFoot[s] = rigPoint(f, at, footTarget(f, sd), false);
    }
    return c;
}

// Tiefste Durchdringung der sichtbaren Körperformen (mit den echten Radien der gezeichneten Teile)
float Player::capePenetration() const {
    if (!capeInit_) return 0.0f;
    const RigFrame f = rigFrame();
    const CapeColliders col = capeColliders(f, body_.position);
    auto depth = [](Vector3 p, Vector3 a, Vector3 b, float radius) {
        Vector3 ab = Vector3Subtract(b, a);
        float len2 = Vector3DotProduct(ab, ab);
        float t = len2 > 1e-6f ? std::clamp(Vector3DotProduct(Vector3Subtract(p, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
        return radius - Vector3Distance(p, Vector3Add(a, Vector3Scale(ab, t)));
    };
    float worst = 0.0f;
    for (int r = 1; r < kClothRows; r++) {
        for (int c = 0; c < kClothCols; c++) {
            const Vector3& p = capePos_[(size_t)(r * kClothCols + c)];
            worst = std::max(worst, depth(p, col.torsoA, col.torsoB, 0.235f));
            worst = std::max(worst, depth(p, col.skirtA, col.skirtB, 0.31f));
            worst = std::max(worst, depth(p, col.shoulderL, col.shoulderR, 0.115f));
            worst = std::max(worst, depth(p, col.legTop[0], col.legFoot[0], 0.105f));
            worst = std::max(worst, depth(p, col.legTop[1], col.legFoot[1], 0.105f));
        }
    }
    return worst;
}

// Stoffsimulation. Jeder Punkt wird von einer Feder zur Ruheform des Mantels gezogen (oben stark, am Saum locker), dazu Schwerkraft,
// Fahrtwind und ein ruhiges Wellen des Stoffs. Nachbarn halten Abstand, und Rumpf, Schultern, Rock und Beine sind feste Kapseln,
// aus denen der Stoff herausgeschoben wird. Die sichtbaren Falten kommen als Wellenprofil obendrauf (siehe capeDraw_).
void Player::simulateCape(float dt) {
    if (dt <= 0.0f) return;
    dt = std::min(dt, 1.0f / 30.0f);
    const RigFrame f = rigFrame();
    const Vector3 at = body_.position;
    constexpr int kN = kClothCols * kClothRows;

    std::array<Vector3, kN> rest;
    for (int r = 0; r < kClothRows; r++)
        for (int c = 0; c < kClothCols; c++) rest[(size_t)(r * kClothCols + c)] = rigPoint(f, at, capeRestLocal(f, c, r), r < 4);

    const float floorY = at.y + 0.03f;
    for (Vector3& q : rest) q.y = std::max(q.y, floorY);   // beim Rollen taucht der Körper unter die Fußhöhe; der Stoff nicht

    if (!capeInit_ || Vector3Distance(capePos_[(size_t)(kClothCols / 2)], rest[(size_t)(kClothCols / 2)]) > 2.5f) {
        capePos_ = rest;
        capePrev_ = rest;
        capeInit_ = true;
    }
    capeTime_ += dt;

    // Abstände der Ruheform (ändern sich nur beim Ducken)
    std::array<float, kN> downLen{}, rightLen{};
    for (int r = 0; r < kClothRows; r++) {
        for (int c = 0; c < kClothCols; c++) {
            size_t i = (size_t)(r * kClothCols + c);
            if (r + 1 < kClothRows) downLen[i] = Vector3Distance(rest[i], rest[i + (size_t)kClothCols]);
            if (c + 1 < kClothCols) rightLen[i] = Vector3Distance(rest[i], rest[i + 1]);
        }
    }

    const Vector3 vel = body_.velocity;
    const float speed = std::hypot(vel.x, vel.z);
    const float dashK = state_ == PlayerState::Dashing ? 1.7f : 1.0f;
    const float g = -9.0f;
    // Unabhängig von der Bildrate: Dämpfung und Schrittgrenze skalieren mit der Zeit, die Verlet-Geschwindigkeit mit dem Zeitverhältnis
    const float k60 = dt * 60.0f;
    const float kMaxStep = 0.15f * k60;   // größte Bewegung eines Stoffpunkts pro Bild (Meter), entspricht 9 m/s
    const float damping = std::pow(0.955f, k60);
    const float dtRatio = capeDtPrev_ > 1e-5f ? dt / capeDtPrev_ : 1.0f;
    capeDtPrev_ = dt;

    for (int r = 1; r < kClothRows; r++) {
        const float k = (float)r / (float)(kClothRows - 1);
        const float stiff = 170.0f - 140.0f * std::pow(k, 0.75f);   // oben stramm, unten locker
        for (int c = 0; c < kClothCols; c++) {
            size_t i = (size_t)(r * kClothCols + c);
            Vector3 p = capePos_[i];
            Vector3 v = Vector3Scale(Vector3Subtract(p, capePrev_[i]), damping * dtRatio);
            float vl = Vector3Length(v);
            if (vl > kMaxStep) v = Vector3Scale(v, kMaxStep / vl);   // nie schneller als 9 m/s pro Bild
            Vector3 acc = Vector3Scale(Vector3Subtract(rest[i], p), stiff);
            acc.y += g;
            // Fahrtwind: gegen die Bewegung; beim Fallen hebt er den Mantel an
            float windK = (0.5f + 1.5f * k) * dashK;
            acc.x -= vel.x * windK;
            acc.z -= vel.z * windK;
            acc.y += std::clamp(-vel.y * 1.5f * k, -6.0f, 26.0f);
            // Ruhiges Wellen, das den Mantel entlang läuft (tiefe Frequenz: kein Zittern)
            float wave = std::sin(capeTime_ * 3.4f - (float)r * 0.55f + (float)c * 0.45f);
            float amp = (0.35f + std::min(speed, 8.0f) * 0.32f) * k;
            acc = Vector3Add(acc, Vector3Scale(f.side, wave * amp));
            acc = Vector3Add(acc, Vector3Scale(f.fwd, std::sin(capeTime_ * 2.3f - (float)r * 0.7f - (float)c * 0.3f) * amp * 0.6f));
            capePrev_[i] = p;
            capePos_[i] = Vector3Add(Vector3Add(p, v), Vector3Scale(acc, dt * dt));
        }
    }

    const CapeColliders col = capeColliders(f, at);

    for (int iter = 0; iter < 5; iter++) {
        for (int c = 0; c < kClothCols; c++) capePos_[(size_t)c] = rest[(size_t)c];
        auto link = [&](size_t a2, size_t b2, float len, bool aFixed) {
            Vector3 d = Vector3Subtract(capePos_[b2], capePos_[a2]);
            float dist = Vector3Length(d);
            if (dist < 1e-5f) return;
            float diff = (dist - len) / dist;
            if (aFixed) {
                capePos_[b2] = Vector3Subtract(capePos_[b2], Vector3Scale(d, diff));
            } else {
                capePos_[a2] = Vector3Add(capePos_[a2], Vector3Scale(d, diff * 0.5f));
                capePos_[b2] = Vector3Subtract(capePos_[b2], Vector3Scale(d, diff * 0.5f));
            }
        };
        for (int r = 0; r < kClothRows - 1; r++)
            for (int c = 0; c < kClothCols; c++) {
                size_t i = (size_t)(r * kClothCols + c);
                link(i, i + (size_t)kClothCols, downLen[i], r == 0);
            }
        for (int r = 1; r < kClothRows; r++)
            for (int c = 0; c < kClothCols - 1; c++) {
                size_t i = (size_t)(r * kClothCols + c);
                link(i, i + 1, rightLen[i], false);
            }
        for (int r = 1; r < kClothRows; r++) {
            for (int c = 0; c < kClothCols; c++) {
                size_t i = (size_t)(r * kClothCols + c);
                Vector3& p = capePos_[i];
                Vector3 moved = Vector3Subtract(p, capePrev_[i]);
                float ml = Vector3Length(moved);
                if (ml > kMaxStep * 1.5f) p = Vector3Add(capePrev_[i], Vector3Scale(moved, kMaxStep * 1.5f / ml));
                pushOutOfCapsule(p, col.torsoA, col.torsoB, 0.26f, f.fwd, true);
                pushOutOfCapsule(p, col.skirtA, col.skirtB, 0.345f, f.fwd, true);
                pushOutOfCapsule(p, col.shoulderL, col.shoulderR, 0.145f, f.fwd, true);
                pushOutOfCapsule(p, col.legTop[0], col.legFoot[0], 0.14f, f.fwd, true);
                pushOutOfCapsule(p, col.legTop[1], col.legFoot[1], 0.14f, f.fwd, true);
                if (p.y < floorY) p.y = floorY;
            }
        }
    }

    // Sichtbare Falten: ein Wellenprofil quer zur Stoffbahn, unten kräftiger; im schnellen Lauf strafft der Wind den Stoff
    const float calm = 1.0f - 0.55f * std::min(1.0f, speed / 7.0f);
    const float runK = std::min(1.0f, speed / 6.0f);
    for (int r = 0; r < kClothRows; r++) {
        const float k = (float)r / (float)(kClothRows - 1);
        for (int c = 0; c < kClothCols; c++) {
            size_t i = (size_t)(r * kClothCols + c);
            auto at2 = [&](int cc, int rr) -> const Vector3& {
                cc = std::clamp(cc, 0, kClothCols - 1);
                rr = std::clamp(rr, 0, kClothRows - 1);
                return capePos_[(size_t)(rr * kClothCols + cc)];
            };
            Vector3 n = Vector3CrossProduct(Vector3Subtract(at2(c, r + 1), at2(c, r - 1)), Vector3Subtract(at2(c + 1, r), at2(c - 1, r)));
            float nl = Vector3Length(n);
            if (nl < 1e-6f) {
                capeDraw_[i] = capePos_[i];
                continue;
            }
            n = Vector3Scale(n, 1.0f / nl);
            // Falten wölben sich nur vom Körper weg (nach hinten), nie in den Rock oder Rücken hinein
            if (Vector3DotProduct(n, f.fwd) > 0.0f) n = Vector3Scale(n, -1.0f);
            const float u = (float)c / (float)(kClothCols - 1);   // 0..1 quer über den Mantel
            float phase = 6.2831853f * 3.0f * u + 1.1f * std::sin(capeTime_ * 0.9f + k * 2.2f) + k * 2.4f;
            float fold = (0.5f + 0.5f * std::sin(phase)) * (0.02f + 0.115f * smooth01(0.0f, 0.7f, k)) * calm;
            fold += (0.5f + 0.5f * std::sin(6.2831853f * 4.5f * u + capeTime_ * 1.7f - k * 3.0f)) * 0.016f * smooth01(0.2f, 1.0f, k) * calm;
            fold += (0.5f + 0.5f * std::sin(capeTime_ * 8.0f - k * 6.5f + u * 5.0f)) * 0.03f * k * runK;   // Wellen im Lauf
            capeDraw_[i] = Vector3Add(capePos_[i], Vector3Scale(n, fold * smooth01(0.0f, 0.14f, k)));
        }
    }
}

void Player::locomotion(float dt, const PlayerInput& in, Vector3 wish, float speedScale, bool allowTurn) {
    float wishLen = Vector3Length(wish);
    float speed = (in.sprint ? cfg_.sprintSpeed : cfg_.walkSpeed) * speedScale * mods_.moveSpeedMult;
    float accel = body_.grounded ? (wishLen > 0.0f ? cfg_.groundAccel : cfg_.groundDecel) : cfg_.airAccel;
    body_.velocity.x = approach(body_.velocity.x, wish.x * speed, accel * dt);
    body_.velocity.z = approach(body_.velocity.z, wish.z * speed, accel * dt);
    if (allowTurn && wishLen > 0.1f) yaw_ = turnToward(yaw_, std::atan2(wish.x, wish.z), cfg_.turnRate * dt);
}

void Player::doJump(EventBus& events) {
    body_.velocity.y = cfg_.jumpSpeed * mods_.jumpMult;
    body_.grounded = false;
    coyote_ = 0.0f;
    jumpBuffer_ = 0.0f;
    jumping_ = true;
    events.emit(PlayerJumped{});
}

void Player::doAirJump(EventBus& events) {
    body_.velocity.y = cfg_.jumpSpeed * 0.92f * mods_.jumpMult;
    airJumpUsed_ = true;
    jumpBuffer_ = 0.0f;
    jumping_ = true;
    events.emit(PlayerAirJumped{});
}

void Player::updateNormal(float dt, const PlayerInput& in, const PlayerFrame& frame, Vector3 wish, EventBus& events) {
    if (comboTimer_ <= 0.0f) comboStep_ = 0;
    coyote_ = body_.grounded ? cfg_.coyoteTime : std::max(0.0f, coyote_ - dt);

    locomotion(dt, in, wish, 1.0f, true);

    if (jumpBuffer_ > 0.0f && coyote_ > 0.0f) {
        doJump(events);
    } else if (in.jumpPressed && doubleJumpUnlocked_ && !airJumpUsed_ && !body_.grounded && frame.level) {
        // Kurz vor der Landung zählt ein Druck als vorgemerkter Bodensprung, nicht als Luftsprung
        float ground = frame.level->groundHeight(body_.position, cfg_.radius, body_.position.y);
        if (body_.velocity.y > 1.0f || body_.position.y - ground > 1.2f) doAirJump(events);
    }
    if (jumping_ && !in.jumpDown && body_.velocity.y > 0.0f) {
        body_.velocity.y *= cfg_.jumpCut;
        jumping_ = false;
    }

    if (dashBuffer_ > 0.0f && tryDash(wish, events)) return;
    if (castBuffer_ > 0.0f) tryCast(frame, events);

    if (dodgeBuffer_ > 0.0f && dodgeCooldown_ <= 0.0f) {
        startDodge(wish, events);
    } else if (attackBuffer_ > 0.0f && weapon_.valid()) {
        startAttack(comboTimer_ > 0.0f ? comboStep_ : 0, frame, wish, events);
    }
}

bool Player::tryDash(Vector3 wish, EventBus& events) {
    if (!dashUnlocked_ || dashCooldown_ > 0.0f) return false;
    if (!body_.grounded && airDashUsed_) return false;
    float wishLen = Vector3Length(wish);
    dashDir_ = wishLen > 0.1f ? Vector3Scale(wish, 1.0f / wishLen) : Vector3{std::sin(yaw_), 0.0f, std::cos(yaw_)};
    yaw_ = std::atan2(dashDir_.x, dashDir_.z);
    state_ = PlayerState::Dashing;
    stateTimer_ = 0.0f;
    dashBuffer_ = 0.0f;
    dashCooldown_ = cfg_.dashCooldown;
    attackBuffer_ = 0.0f;
    airDashUsed_ = !body_.grounded;
    jumping_ = false;
    body_.velocity.y = 0.0f;
    events.emit(PlayerDashed{});
    return true;
}

void Player::updateDashing(float dt) {
    stateTimer_ += dt;
    body_.velocity.x = dashDir_.x * cfg_.dashSpeed;
    body_.velocity.z = dashDir_.z * cfg_.dashSpeed;
    body_.velocity.y = 0.0f;
    if (stateTimer_ >= cfg_.dashDuration) {
        state_ = PlayerState::Normal;
        body_.velocity.x *= 0.35f;
        body_.velocity.z *= 0.35f;
    }
}

void Player::tryCast(const PlayerFrame& frame, EventBus& events) {
    if (sparkCooldown_ > 0.0f) return;
    Vector3 dir{std::sin(frame.cameraYaw), 0.0f, std::cos(frame.cameraYaw)};
    yaw_ = frame.cameraYaw;  // die Figur schaut dorthin, wohin die Kamera zeigt
    castDirection_ = dir;
    castOrigin_ = {body_.position.x + dir.x * 0.7f, body_.position.y + cfg_.height * 0.62f, body_.position.z + dir.z * 0.7f};
    castRequested_ = true;
    castBuffer_ = 0.0f;
    castGlow_ = 0.25f;
    sparkCooldown_ = cfg_.sparkCooldown * mods_.sparkCooldownMult;
    events.emit(PlayerCast{});
}

void Player::startAttack(int step, const PlayerFrame& frame, Vector3 wish, EventBus& events) {
    comboStep_ = std::clamp(step, 0, (int)weapon_.attacks.size() - 1);
    state_ = PlayerState::Attacking;
    phase_ = AttackPhase::Windup;
    stateTimer_ = 0.0f;
    attackBuffer_ = 0.0f;
    comboTimer_ = 0.0f;
    swingId_++;

    // Blickrichtung beim Angriffsstart: Eingaberichtung, sonst der nächste Gegner. Zeigt die Eingabe
    // grob zum Gegner (unter 80 Grad Abweichung), zielt der Schlag auf ihn.
    std::optional<float> toTarget;
    if (frame.assistTarget) {
        toTarget = std::atan2(frame.assistTarget->x - body_.position.x, frame.assistTarget->z - body_.position.z);
    }
    bool hasInput = Vector3Length(wish) > 0.1f;
    if (hasInput) {
        float inputYaw = std::atan2(wish.x, wish.z);
        bool towardTarget = toTarget && std::fabs(wrapAngle(*toTarget - inputYaw)) < 80.0f * DEG2RAD;
        yaw_ = towardTarget ? *toTarget : inputYaw;
    } else if (toTarget) {
        yaw_ = *toTarget;
    }
    events.emit(PlayerAttacked{comboStep_});
}

void Player::endAttack() {
    state_ = PlayerState::Normal;
    if (comboStep_ + 1 < (int)weapon_.attacks.size()) {
        comboStep_++;
        comboTimer_ = weapon_.comboWindow;
    } else {
        comboStep_ = 0;
        comboTimer_ = 0.0f;
    }
}

void Player::updateAttacking(float dt, const PlayerInput& in, const PlayerFrame& frame, Vector3 wish, EventBus& events) {
    stateTimer_ += dt;
    const AttackDef& a = currentAttack();
    AttackTimes t = attackTimes(a);
    float now = stateTimer_;
    phase_ = now < t.windup ? AttackPhase::Windup : (now < t.windup + t.active ? AttackPhase::Active : AttackPhase::Recovery);

    Vector3 facing{std::sin(yaw_), 0.0f, std::cos(yaw_)};
    switch (phase_) {
        case AttackPhase::Windup: locomotion(dt, in, wish, a.moveScale, false); break;
        case AttackPhase::Active: {
            float lungeSpeed = t.active > 0.0f ? a.lunge / t.active : 0.0f;
            body_.velocity.x = facing.x * lungeSpeed;
            body_.velocity.z = facing.z * lungeSpeed;
            break;
        }
        case AttackPhase::Recovery:
            body_.velocity.x = approach(body_.velocity.x, 0.0f, cfg_.groundDecel * dt);
            body_.velocity.z = approach(body_.velocity.z, 0.0f, cfg_.groundDecel * dt);
            // Aus dem Nachschwingen heraus geht es sofort weiter: Dash, Ausweichen oder nächste Kombo-Stufe
            if (dashBuffer_ > 0.0f && tryDash(wish, events)) return;
            if (dodgeBuffer_ > 0.0f && dodgeCooldown_ <= 0.0f) {
                startDodge(wish, events);
                return;
            }
            if (attackBuffer_ > 0.0f && comboStep_ + 1 < (int)weapon_.attacks.size()) {
                startAttack(comboStep_ + 1, frame, wish, events);
                return;
            }
            break;
    }

    if (now >= t.total()) endAttack();
}

void Player::startDodge(Vector3 wish, EventBus& events) {
    float wishLen = Vector3Length(wish);
    if (wishLen > 0.1f) {
        dodgeDir_ = Vector3Scale(wish, 1.0f / wishLen);
        yaw_ = std::atan2(dodgeDir_.x, dodgeDir_.z);
    } else {
        dodgeDir_ = {-std::sin(yaw_), 0.0f, -std::cos(yaw_)};  // ohne Eingabe: Schritt zurück
    }
    state_ = PlayerState::Dodging;
    stateTimer_ = 0.0f;
    attackBuffer_ = dodgeBuffer_ = 0.0f;
    comboTimer_ = 0.0f;
    comboStep_ = 0;
    events.emit(PlayerDodged{});
}

void Player::updateDodging(float dt) {
    stateTimer_ += dt;
    float progress = std::clamp(stateTimer_ / cfg_.dodgeDuration, 0.0f, 1.0f);
    float easing = progress < 0.7f ? 1.0f : 1.0f - (progress - 0.7f) / 0.3f * 0.6f;
    body_.velocity.x = dodgeDir_.x * cfg_.dodgeSpeed * easing;
    body_.velocity.z = dodgeDir_.z * cfg_.dodgeSpeed * easing;
    if (stateTimer_ >= cfg_.dodgeDuration) {
        state_ = PlayerState::Normal;
        dodgeCooldown_ = cfg_.dodgeCooldown * mods_.dodgeCooldownMult;
        body_.velocity.x *= 0.4f;
        body_.velocity.z *= 0.4f;
    }
}

void Player::updateHurt(float dt) {
    stateTimer_ += dt;
    body_.velocity.x = approach(body_.velocity.x, 0.0f, 25.0f * dt);
    body_.velocity.z = approach(body_.velocity.z, 0.0f, 25.0f * dt);
    if (stateTimer_ >= cfg_.hurtTime) state_ = PlayerState::Normal;
}

// ---------------------------------------------------------------- Zeichnen

void Player::draw(const LitRenderer& r, const Level& level) const {
    const float rad = cfg_.radius;
    const Vector3 p = body_.position;
    const bool dodging = state_ == PlayerState::Dodging;
    const bool dashing = state_ == PlayerState::Dashing;

    // Schatten: kleiner und blasser, je höher man ist
    float ground = level.groundHeight(p, rad, p.y);
    if (ground > -1000.0f) {
        float height = std::max(0.0f, p.y - ground);
        float scale = 1.15f / (1.0f + height * 0.25f);
        unsigned char alpha = (unsigned char)std::clamp(95.0f - height * 12.0f, 25.0f, 95.0f);
        r.cylinder({p.x, ground + 0.02f, p.z}, rad * scale, 0.01f, Color{0, 0, 0, alpha});
    }

    // Nach einem Treffer blinkt der Spieler
    if (health_.invulnerable() && !dead() && std::fmod(GetTime(), 0.16) < 0.08) return;

    Color tunic{70, 100, 170, 255};
    Color skin{244, 208, 170, 255};
    Color hair{120, 76, 44, 255};
    Color pants{112, 98, 86, 255};
    Color boots{112, 78, 58, 255};
    Color leather{128, 88, 58, 255};
    Color cape{188, 52, 62, 255};
    Color trim{236, 198, 96, 255};
    Color blade{226, 232, 242, 255};
    const bool flash = state_ == PlayerState::Hurt && stateTimer_ < 0.12f;
    if (flash) tunic = cape = pants = boots = leather = Color{235, 85, 85, 255};
    else if (dead()) tunic = skin = hair = pants = boots = leather = cape = Color{120, 120, 130, 255};
    // Beim Trefferblitz und im Tod einfarbig, sonst mit Texturen
    auto mat = [&](const char* name) { return (flash || dead()) ? kNoMaterial : r.material(name); };

    const RigFrame f = rigFrame();
    const Vector3 fwd = f.fwd, side = f.side;
    const Vector3 up{0, 1, 0};
    const float moveK = f.moveK;
    const float pose = f.pose;
    const float t = (float)GetTime();

    // Schwert: in Ruhe seitlich gehalten, im Angriff über den Schwungbogen der aktuellen Stufe geführt
    float angle = -0.9f;
    float tilt = -0.5f;   // Neigung der Klinge nach oben (+) oder unten (-)
    bool slashing = false;
    float sweepT = 0.0f;
    if (state_ == PlayerState::Attacking) {
        const AttackDef& a = currentAttack();
        AttackTimes tm = attackTimes(a);
        float now = stateTimer_;
        if (now < tm.windup) {
            float k = tm.windup > 0.0f ? now / tm.windup : 1.0f;
            float pull = a.sweepFrom + (a.sweepFrom < a.sweepTo ? -0.35f : 0.35f);
            angle = -0.9f + (pull - -0.9f) * easeOut(k);
            tilt = -0.5f + 1.1f * easeOut(k);
        } else if (now < tm.windup + tm.active) {
            sweepT = std::clamp((now - tm.windup) / std::max(tm.active, 0.001f), 0.0f, 1.0f);
            angle = a.sweepFrom + (a.sweepTo - a.sweepFrom) * easeOut(sweepT);
            tilt = 0.6f - 0.75f * sweepT;
            slashing = true;
        } else {
            float k = std::clamp((now - tm.windup - tm.active) / std::max(tm.recovery, 0.001f), 0.0f, 1.0f);
            angle = a.sweepTo + (-0.9f - a.sweepTo) * k * k;
            tilt = -0.15f - 0.35f * k;
        }
    }
    const float bladeLength = 1.1f * mods_.rangeMult;

    auto drawHero = [&](Vector3 at, unsigned char alpha, bool withSword) {
        auto A = [alpha](Color c) {
            c.a = alpha;
            return c;
        };
        r.setAnchor(at, yaw_);

        const float hipY = f.hipY;
        const float breathe = std::sin(t * 2.2f) * 0.01f * (1.0f - moveK);
        auto W = [&](Vector3 l, bool upper) { return rigPoint(f, at, l, upper); };

        const float phase = animPhase_;
        const bool inAir = !body_.grounded && !dashing;
        const float vy = body_.velocity.y;

        // ---- Beine: Oberschenkel und Schienbein verjüngt, Stiefel mit Schaft, Umschlag und Kappe
        for (float s : {-1.0f, 1.0f}) {
            Vector3 hip = W({s * 0.13f, hipY - 0.03f, 0}, false);
            Vector3 knee, ankle;
            twoBone(hip, W(footTarget(f, s), false), 0.44f, 0.44f, fwd, knee, ankle);
            r.useMaterial(mat("hero_pants"));
            r.taper(hip, knee, 0.125f, 0.092f, A(pants));
            r.taper(knee, ankle, 0.092f, 0.072f, A(pants));
            r.clearMaterial();
            r.useMaterial(mat("hero_leather"));
            Vector3 shin = Vector3Subtract(knee, ankle);
            Vector3 shaftTop = Vector3Add(ankle, Vector3Scale(Vector3Normalize(shin), 0.30f));
            r.taper(Vector3Add(ankle, {0, 0.02f, 0}), shaftTop, 0.085f, 0.105f, A(boots));
            r.taper(Vector3Add(shaftTop, Vector3Scale(Vector3Normalize(shin), -0.03f)), shaftTop, 0.112f, 0.112f, A(leather), false);
            Vector3 heel = {ankle.x, ankle.y + 0.045f, ankle.z};
            Vector3 toe = Vector3Add(heel, Vector3Scale(fwd, 0.19f));
            r.taper(heel, toe, 0.098f, 0.075f, A(boots));
            r.clearMaterial();
            r.useMaterial(mat("hero_gold"));
            r.sphere(Vector3Add(knee, Vector3Scale(fwd, 0.075f)), 0.052f, A(trim));   // Knieschutz
            r.clearMaterial();
        }

        // ---- Tunika: Rock, Rumpf, Gürtel, Schultern
        Vector3 pelvis = W({0, hipY, 0}, false);
        Vector3 waist = W({0, hipY + 0.20f + breathe, 0}, true);
        Vector3 chest = W({0, hipY + 0.40f * pose + breathe, 0}, true);
        r.useMaterial(mat("hero_tunic"));
        // Der Rock schwingt mit den Beinen ein wenig nach vorn und hinten
        float skirtSwing = std::sin(phase) * 0.05f * moveK;
        r.taper(W({0, hipY - 0.27f, skirtSwing}, false), W({0, hipY + 0.03f, 0}, false), 0.315f, 0.245f, A(tunic), false);
        r.taper(Vector3Add(pelvis, {0, 0.02f, 0}), waist, 0.235f, 0.205f, A(tunic));
        r.taper(waist, chest, 0.205f, 0.265f, A(tunic));
        r.ellipsoid(W({0, hipY + 0.42f * pose + breathe, 0.01f}, true), {0.30f, 0.23f, 0.235f}, A(tunic), yaw_);
        Vector3 shL = W({-0.335f, hipY + 0.45f * pose + breathe, 0}, true), shR = W({0.335f, hipY + 0.45f * pose + breathe, 0}, true);
        r.limb(shL, shR, 0.11f, A(tunic));
        r.clearMaterial();
        // Saum und Kragen der Tunika in Gold
        r.useMaterial(mat("hero_gold"));
        r.taper(W({0, hipY - 0.275f + skirtSwing * 0.02f, skirtSwing}, false), W({0, hipY - 0.245f, skirtSwing}, false), 0.322f, 0.318f, A(trim), false);
        r.clearMaterial();
        // Gürtel mit Schnalle und Tasche, dazu der Heiltrank
        r.useMaterial(mat("hero_leather"));
        r.cylinder(Vector3Add(pelvis, {0, -0.02f, 0}), 0.262f, 0.095f, A(leather));
        r.ellipsoid(W({0.27f, hipY - 0.06f, -0.04f}, false), {0.08f, 0.10f, 0.07f}, A(boots), yaw_);
        r.clearMaterial();
        r.useMaterial(mat("hero_gold"));
        r.bar(W({-0.05f, hipY + 0.03f, 0.262f}, false), W({0.05f, hipY + 0.03f, 0.262f}, false), 0.07f, 0.03f, A(trim), up);
        r.clearMaterial();
        if (!flash && !dead()) {
            r.ellipsoid(W({-0.27f, hipY - 0.08f, 0.06f}, false), {0.045f, 0.06f, 0.045f}, Color{70, 175, 105, 255}, yaw_);
        }
        // Schulterstücke aus Leder mit goldenem Rand
        r.useMaterial(mat("hero_leather"));
        r.ellipsoid(shL, {0.15f, 0.10f, 0.15f}, A(leather), yaw_);
        r.ellipsoid(shR, {0.15f, 0.10f, 0.15f}, A(leather), yaw_);
        r.clearMaterial();
        r.useMaterial(mat("hero_gold"));
        r.taper(Vector3Add(shL, {0, 0.055f, 0}), Vector3Add(shL, {0, 0.075f, 0}), 0.105f, 0.10f, A(trim), false);
        r.taper(Vector3Add(shR, {0, 0.055f, 0}), Vector3Add(shR, {0, 0.075f, 0}), 0.105f, 0.10f, A(trim), false);
        r.clearMaterial();

        // ---- Hals und Kopf
        Vector3 neckBase = W({0, hipY + 0.52f * pose + breathe, 0}, true);
        Vector3 neckTop = W({0, hipY + 0.66f * pose + breathe, 0.01f}, true);
        r.useMaterial(mat("hero_skin"));
        r.taper(neckBase, neckTop, 0.085f, 0.075f, A(skin));
        r.clearMaterial();
        r.useMaterial(mat("hero_tunic"));
        r.taper(Vector3Add(neckBase, {0, -0.03f, 0}), Vector3Add(neckBase, {0, 0.04f, 0}), 0.15f, 0.115f, A(tunic), false);
        r.clearMaterial();
        Vector3 head = Vector3Add(neckTop, Vector3Add({0, 0.16f, 0}, Vector3Scale(fwd, 0.03f)));
        r.useMaterial(mat("hero_skin"));
        r.ellipsoid(head, {0.205f, 0.235f, 0.215f}, A(skin), yaw_);
        for (float sd : {-1.0f, 1.0f}) {
            r.sphere(Vector3Add(head, Vector3Add(Vector3Scale(side, sd * 0.205f), {0, -0.01f, 0})), 0.045f, A(skin));   // Ohren
        }
        r.sphere(Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.215f), {0, -0.035f, 0})), 0.036f, A(skin));            // Nase
        r.clearMaterial();
        // Haare: Kappe, Stirnfransen, Seitenlocken und ein Zopf, der im Lauf mitschwingt
        r.useMaterial(mat("hero_hair"));
        r.ellipsoid(Vector3Add(head, Vector3Add(Vector3Scale(fwd, -0.05f), {0, 0.07f, 0})), {0.225f, 0.20f, 0.24f}, A(hair), yaw_);
        r.ellipsoid(Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.13f), {0, 0.155f, 0})), {0.19f, 0.06f, 0.09f}, A(hair), yaw_);
        for (float sd : {-1.0f, 1.0f}) {
            r.ellipsoid(Vector3Add(head, Vector3Add(Vector3Scale(side, sd * 0.19f), Vector3Add(Vector3Scale(fwd, 0.03f), {0, -0.02f, 0}))), {0.04f, 0.10f, 0.075f}, A(hair), yaw_);
        }
        float sway = std::sin(phase) * 0.06f * moveK + std::sin(t * 1.7f) * 0.012f;
        Vector3 tail0 = Vector3Add(head, Vector3Add(Vector3Scale(fwd, -0.19f), {0, 0.02f, 0}));
        Vector3 tail1 = Vector3Add(tail0, Vector3Add(Vector3Scale(fwd, -0.12f - 0.10f * moveK), Vector3Add(Vector3Scale(side, sway), {0, -0.16f + 0.07f * moveK, 0})));
        Vector3 tail2 = Vector3Add(tail1, Vector3Add(Vector3Scale(fwd, -0.05f - 0.06f * moveK), Vector3Add(Vector3Scale(side, sway * 1.6f), {0, -0.17f + 0.06f * moveK, 0})));
        r.taper(tail0, tail1, 0.065f, 0.052f, A(hair));
        r.taper(tail1, tail2, 0.052f, 0.02f, A(hair));
        r.clearMaterial();
        // Gesicht: Augen mit Iris, Brauen, Mund
        for (float sd : {-1.0f, 1.0f}) {
            Vector3 eye = Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.18f), Vector3Add(Vector3Scale(side, sd * 0.078f), {0, 0.025f, 0})));
            r.sphere(eye, 0.042f, A(Color{240, 240, 236, 255}));
            r.sphere(Vector3Add(eye, Vector3Scale(fwd, 0.028f)), 0.026f, A(Color{40, 60, 90, 255}));
            r.sphere(Vector3Add(eye, Vector3Add(Vector3Scale(fwd, 0.04f), {0, 0.006f, 0})), 0.012f, A(Color{20, 20, 26, 255}));
            Vector3 b0 = Vector3Add(eye, Vector3Add(Vector3Scale(side, -sd * 0.04f), {0, 0.055f, 0.0f}));
            Vector3 b1 = Vector3Add(eye, Vector3Add(Vector3Scale(side, sd * 0.045f), {0, 0.07f, 0.0f}));
            r.bar(b0, b1, 0.02f, 0.018f, A(Color{88, 54, 32, 255}), up);
        }
        r.bar(Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.2f), Vector3Add(Vector3Scale(side, -0.035f), {0, -0.105f, 0}))),
              Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.2f), Vector3Add(Vector3Scale(side, 0.035f), {0, -0.105f, 0}))), 0.014f, 0.014f,
              A(Color{150, 80, 70, 255}), up);

        // ---- Umhang (simulierter Stoff) mit goldenem Saum und Spangen
        if (capeInit_) {
            std::array<Vector3, kClothCols * kClothRows> shifted = capeDraw_;
            Vector3 shift{at.x - p.x, at.y - p.y, at.z - p.z};
            for (Vector3& q : shifted) q = Vector3Add(q, shift);
            r.useMaterial(mat("hero_cape"));
            r.cloth(shifted.data(), A(cape));
            r.clearMaterial();
            r.useMaterial(mat("hero_gold"));
            const int hem = kClothRows - 1;
            for (int c = 0; c + 1 < kClothCols; c++) {
                r.limb(shifted[(size_t)(hem * kClothCols + c)], shifted[(size_t)(hem * kClothCols + c + 1)], 0.016f, A(trim));
            }
            for (int c : {0, kClothCols - 1}) {
                Vector3 q = shifted[(size_t)c];
                r.sphere({q.x, q.y + 0.01f, q.z}, 0.06f, A(trim));
            }
            r.clearMaterial();
        }

        // ---- Arme: Ärmel, Lederstulpe, Hand; der linke schwingt im Lauf, der rechte führt das Schwert
        for (float s : {-1.0f, 1.0f}) {
            Vector3 shoulder = s > 0.0f ? shR : shL;
            Vector3 hand;
            bool sword = withSword && s > 0.0f && weapon_.valid() && !dead();
            if (sword) {
                // Griffposition: dort, wo die Klinge ansetzt (der Arm folgt dem Bogen so weit er reicht)
                Vector3 d = dirFromAngle(yaw_ + angle);
                hand = {at.x + d.x * 0.35f, at.y + 1.0f * pose, at.z + d.z * 0.35f};
            } else {
                float phiL = phase + (s > 0.0f ? kPi : 0.0f);   // Arm schwingt gegen das Bein derselben Seite
                float swing = -std::sin(phiL) * 0.34f * moveK;
                Vector3 local;
                if (dodging) local = {s * 0.20f, hipY + 0.30f, 0.22f};
                else if (inAir) local = {s * 0.34f, hipY + 0.42f + 0.20f * std::clamp(vy / 9.0f, -1.0f, 1.0f), 0.10f};
                else local = {s * 0.30f, hipY + 0.06f + breathe * 4.0f, swing + 0.04f};
                hand = W(local, true);
            }
            Vector3 elbow, wrist;
            Vector3 bend = Vector3Add(Vector3Scale(side, s * 0.6f), Vector3Add({0, -0.25f, 0}, Vector3Scale(fwd, -0.25f)));
            twoBone(shoulder, hand, 0.30f, 0.28f, bend, elbow, wrist);
            r.useMaterial(mat("hero_tunic"));
            r.taper(shoulder, elbow, 0.098f, 0.078f, A(tunic));
            r.clearMaterial();
            r.useMaterial(mat("hero_leather"));
            Vector3 fore = Vector3Subtract(wrist, elbow);
            r.taper(elbow, Vector3Add(elbow, Vector3Scale(fore, 0.85f)), 0.078f, 0.062f, A(leather));   // Stulpe
            r.clearMaterial();
            r.useMaterial(mat("hero_skin"));
            r.sphere(wrist, 0.068f, A(skin));
            r.clearMaterial();

            if (sword) {
                // Schwert: Griff, Knauf, Parierstange, Klinge mit Hohlkehle und Spitze (Stahl)
                Vector3 d = dirFromAngle(yaw_ + angle);
                Vector3 bd = Vector3Normalize({d.x * std::cos(tilt), std::sin(tilt), d.z * std::cos(tilt)});
                Vector3 base = wrist;
                Vector3 perp = Vector3Normalize(Vector3CrossProduct(up, bd));
                Vector3 tip = Vector3Add(base, Vector3Scale(bd, bladeLength));
                r.useMaterial(mat("hero_leather"));
                r.limb(Vector3Subtract(base, Vector3Scale(bd, 0.15f)), Vector3Add(base, Vector3Scale(bd, 0.03f)), 0.034f, A(Color{110, 80, 60, 255}));
                r.clearMaterial();
                r.useMaterial(mat("hero_gold"));
                r.sphere(Vector3Subtract(base, Vector3Scale(bd, 0.17f)), 0.048f, A(trim));
                r.bar(Vector3Subtract(Vector3Add(base, Vector3Scale(bd, 0.04f)), Vector3Scale(perp, 0.17f)), Vector3Add(Vector3Add(base, Vector3Scale(bd, 0.04f)), Vector3Scale(perp, 0.17f)), 0.05f, 0.055f, A(trim), up);
                r.clearMaterial();
                r.useMaterial(mat("hero_steel"));
                Vector3 b0 = Vector3Add(base, Vector3Scale(bd, 0.06f));
                Vector3 b1 = Vector3Add(base, Vector3Scale(bd, bladeLength * 0.82f));
                r.bar(b0, b1, 0.115f, 0.026f, A(blade), up);
                r.bar(b1, Vector3Add(base, Vector3Scale(bd, bladeLength * 0.93f)), 0.078f, 0.024f, A(blade), up);
                r.bar(Vector3Add(base, Vector3Scale(bd, bladeLength * 0.93f)), Vector3Add(tip, Vector3Scale(bd, 0.02f)), 0.036f, 0.02f, A(blade), up);
                r.bar(Vector3Add(b0, Vector3Scale(bd, 0.10f)), Vector3Subtract(b1, Vector3Scale(bd, 0.06f)), 0.03f, 0.034f, A(Color{150, 158, 172, 255}), up);
                r.clearMaterial();
            }
        }
        r.clearAnchor();
    };

    if (dashing) {
        for (int i = 3; i >= 1; i--) {
            Vector3 back{p.x - dashDir_.x * 0.7f * (float)i, p.y, p.z - dashDir_.z * 0.7f * (float)i};
            drawHero(back, (unsigned char)(120 - 30 * i), false);
        }
    }
    drawHero(p, dodging ? 170 : 255, true);

    // Funkenwurf: kurzes Leuchten an der Hand
    if (castGlow_ > 0.0f) {
        float k = castGlow_ / 0.25f;
        r.sphere({p.x + fwd.x * 0.75f, p.y + 1.1f, p.z + fwd.z * 0.75f}, 0.12f + 0.2f * k,
                 Color{255, (unsigned char)(150 + 80 * k), 60, (unsigned char)(120 + 100 * k)});
    }

    if (dead() || !weapon_.valid()) return;

    // Kurze Schleifspur hinter der Klinge während der Trefferphase
    if (slashing) {
        const AttackDef& a = currentAttack();
        for (int i = 1; i <= 4; i++) {
            float trailT = std::max(0.0f, sweepT - 0.09f * (float)i);
            float ta = a.sweepFrom + (a.sweepTo - a.sweepFrom) * easeOut(trailT);
            Vector3 d = dirFromAngle(yaw_ + ta);
            Vector3 base{p.x + d.x * 0.35f, p.y + 1.0f * pose, p.z + d.z * 0.35f};
            Color c{226, 232, 242, (unsigned char)(255 * (0.42f - 0.09f * (float)i))};
            r.box({base.x + d.x * bladeLength * 0.5f, base.y, base.z + d.z * bladeLength * 0.5f}, {0.10f, 0.05f, bladeLength}, c, yaw_ + ta);
        }
    }
}

}  // namespace aldoria
