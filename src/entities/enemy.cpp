#include "entities/enemy.h"

#include "entities/rig.h"

#include <algorithm>
#include <cmath>

#include "core/log.h"
#include "core/math_util.h"
#include "raymath.h"

namespace aldoria {
namespace {

constexpr float kGravity = 28.0f;
constexpr float kMaxFall = 40.0f;
constexpr float kLeapTotalTime = 1.3f;
constexpr float kMaxDrop = 1.2f;   // tiefer als das gilt als Abgrund

Color lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), 255};
}

bool hasLineOfSight(const Level& level, Vector3 from, Vector3 to) {
    Vector3 d = Vector3Subtract(to, from);
    float len = Vector3Length(d);
    if (len < 0.01f) return true;
    float hit;
    return !level.raycast(from, Vector3Scale(d, 1.0f / len), len, hit);
}

}  // namespace

Enemy::Enemy(const EnemyDef* def, Vector3 spawn, float spawnDelay) : def_(def), health_(def->maxHealth) {
    body_.position = spawn;
    spawnPos_ = lastSafe_ = spawn;
    body_.radius = def->radius;
    body_.height = def->height;
    shootTimer_ = 0.6f + def->shootCooldown * 0.3f;  // Fernkämpfer schießen nicht sofort beim Auftauchen
    leapTimer_ = 0.8f;
    strafeDir_ = spawn.x > 0 ? 1.0f : -1.0f;
    if (spawnDelay > 0.0f) {
        state_ = EnemyState::Spawning;
        timer_ = spawnDuration_ = spawnDelay;
    }
}

void Enemy::brake(float dt, float rate) {
    body_.velocity.x = approach(body_.velocity.x, 0.0f, rate * dt);
    body_.velocity.z = approach(body_.velocity.z, 0.0f, rate * dt);
}

float Enemy::telegraphProgress() const {
    if (state_ != EnemyState::Telegraph || def_->telegraph <= 0.0f) return 0.0f;
    return std::clamp(1.0f - timer_ / def_->telegraph, 0.0f, 1.0f);
}

void Enemy::resetAggro() {
    if (state_ == EnemyState::Dead || state_ == EnemyState::Spawning) return;
    state_ = EnemyState::Idle;
    body_.velocity.x = body_.velocity.z = 0.0f;
}

bool Enemy::consumeShot(Vector3& origin, Vector3& direction) {
    if (!shotPending_) return false;
    shotPending_ = false;
    origin = shotOrigin_;
    direction = shotDirection_;
    return true;
}

void Enemy::updateChase(float dt, const Level& level, Vector3 playerPos, Vector3 dir, float dist, float yawToPlayer) {
    yaw_ = turnToward(yaw_, yawToPlayer, 9.0f * dt);

    switch (def_->behavior) {
        case EnemyBehavior::Melee:
            body_.velocity.x = approach(body_.velocity.x, dir.x * def_->chaseSpeed, 25.0f * dt);
            body_.velocity.z = approach(body_.velocity.z, dir.z * def_->chaseSpeed, 25.0f * dt);
            if (dist <= def_->attackRange) {
                state_ = EnemyState::Telegraph;
                timer_ = def_->telegraph;
            }
            break;

        case EnemyBehavior::Ranged: {
            strafeTimer_ -= dt;
            if (strafeTimer_ <= 0.0f) {
                strafeTimer_ = 1.2f + (float)GetRandomValue(0, 120) / 100.0f;
                strafeDir_ = -strafeDir_;
            }
            Vector3 desired{0, 0, 0};
            if (dist < def_->preferredMin) desired = Vector3Scale(dir, -def_->chaseSpeed);
            else if (dist > def_->preferredMax) desired = Vector3Scale(dir, def_->chaseSpeed);
            else desired = Vector3Scale(Vector3{-dir.z, 0.0f, dir.x}, strafeDir_ * def_->chaseSpeed * 0.6f);
            body_.velocity.x = approach(body_.velocity.x, desired.x, 25.0f * dt);
            body_.velocity.z = approach(body_.velocity.z, desired.z, 25.0f * dt);

            shootTimer_ -= dt;
            Vector3 eye{body_.position.x, body_.position.y + def_->height * 0.7f, body_.position.z};
            Vector3 target{playerPos.x, playerPos.y + 1.0f, playerPos.z};
            if (shootTimer_ <= 0.0f && dist <= def_->sightRange && hasLineOfSight(level, eye, target)) {
                state_ = EnemyState::Telegraph;
                timer_ = def_->telegraph;
            }
            break;
        }

        case EnemyBehavior::Leaper:
            body_.velocity.x = approach(body_.velocity.x, dir.x * def_->chaseSpeed, 25.0f * dt);
            body_.velocity.z = approach(body_.velocity.z, dir.z * def_->chaseSpeed, 25.0f * dt);
            leapTimer_ -= dt;
            if (leapTimer_ <= 0.0f && dist <= def_->leapRange && dist > 1.8f && leapSafe(level, yawToPlayer)) {
                state_ = EnemyState::Telegraph;
                timer_ = def_->telegraph;
            }
            break;
    }
}

void Enemy::update(float dt, const Level& level, Vector3 playerPos, bool playerAlive) {
    if (dt <= 0.0f) return;
    flashTimer_ = std::max(0.0f, flashTimer_ - dt);
    poiseCooldown_ = std::max(0.0f, poiseCooldown_ - dt);
    health_.update(dt);
    if (body_.grounded) walkPhase_ += dt * std::hypot(body_.velocity.x, body_.velocity.z) * 2.6f / std::max(0.4f, def_->height);

    Vector3 pos = body_.position;
    Vector3 toPlayer{playerPos.x - pos.x, 0.0f, playerPos.z - pos.z};
    float dist = Vector3Length(toPlayer);
    Vector3 dir = dist > 1e-4f ? Vector3Scale(toPlayer, 1.0f / dist) : Vector3{std::sin(yaw_), 0.0f, std::cos(yaw_)};
    float yawToPlayer = std::atan2(dir.x, dir.z);

    switch (state_) {
        case EnemyState::Spawning:
            body_.velocity.x = body_.velocity.z = 0.0f;
            timer_ -= dt;
            yaw_ = turnToward(yaw_, yawToPlayer, 6.0f * dt);
            if (timer_ <= 0.0f) state_ = playerAlive ? EnemyState::Chase : EnemyState::Idle;
            break;

        case EnemyState::Idle:
            brake(dt, 30.0f);
            if (playerAlive && dist < def_->sightRange) state_ = EnemyState::Chase;
            break;

        case EnemyState::Chase:
            if (!playerAlive || dist > def_->loseRange) {
                state_ = EnemyState::Idle;
                break;
            }
            updateChase(dt, level, playerPos, dir, dist, yawToPlayer);
            break;

        case EnemyState::Telegraph:
            brake(dt, 40.0f);
            // Bis kurz vor dem Ausfall wird gezielt; die letzten 30 % sind festgelegt, damit man ausweichen kann
            if (playerAlive && timer_ > def_->telegraph * 0.3f) yaw_ = turnToward(yaw_, yawToPlayer, 12.0f * dt);
            timer_ -= dt;
            if (timer_ <= 0.0f) {
                if (def_->behavior == EnemyBehavior::Ranged) {
                    shotOrigin_ = {pos.x + std::sin(yaw_) * def_->radius, pos.y + def_->height * 0.65f,
                                   pos.z + std::cos(yaw_) * def_->radius};
                    shotDirection_ = {std::sin(yaw_), 0.0f, std::cos(yaw_)};
                    shotPending_ = true;
                    shootTimer_ = def_->shootCooldown;
                    state_ = EnemyState::Recover;
                    timer_ = def_->attackRecovery;
                } else if (def_->behavior == EnemyBehavior::Leaper && !leapSafe(level, yaw_)) {
                    // Der Landeplatz ist inzwischen kein fester Boden mehr (Spieler bewegt sich, Lücke): nicht springen
                    state_ = EnemyState::Recover;
                    timer_ = 0.35f;
                    leapTimer_ = 0.8f;
                } else if (def_->behavior == EnemyBehavior::Leaper) {
                    state_ = EnemyState::Attack;
                    timer_ = kLeapTotalTime;
                    attackConsumed_ = false;
                    leapTimer_ = def_->leapCooldown;
                    body_.velocity.x = std::sin(yaw_) * def_->leapSpeed;
                    body_.velocity.z = std::cos(yaw_) * def_->leapSpeed;
                    body_.velocity.y = def_->leapHeight;
                    body_.grounded = false;
                } else {
                    state_ = EnemyState::Attack;
                    timer_ = def_->attackActive;
                    attackConsumed_ = false;
                    body_.velocity.x = std::sin(yaw_) * def_->attackLungeSpeed;
                    body_.velocity.z = std::cos(yaw_) * def_->attackLungeSpeed;
                }
            }
            break;

        case EnemyState::Attack:
            timer_ -= dt;
            if (def_->behavior == EnemyBehavior::Leaper) {
                // Der Sprung endet bei der Landung (nach kurzer Startphase)
                if ((timer_ < kLeapTotalTime - 0.2f && body_.grounded) || timer_ <= 0.0f) {
                    state_ = EnemyState::Recover;
                    timer_ = def_->attackRecovery;
                }
            } else if (timer_ <= 0.0f) {
                state_ = EnemyState::Recover;
                timer_ = def_->attackRecovery;
            }
            break;

        case EnemyState::Recover:
            brake(dt, 30.0f);
            timer_ -= dt;
            if (timer_ <= 0.0f) state_ = playerAlive ? EnemyState::Chase : EnemyState::Idle;
            break;

        case EnemyState::Hurt:
            brake(dt, 12.0f);
            timer_ -= dt;
            if (timer_ <= 0.0f) state_ = playerAlive ? EnemyState::Chase : EnemyState::Idle;
            break;

        case EnemyState::Dead:
            brake(dt, 10.0f);
            deadTimer_ += dt;
            break;
    }

    guardEdges(level);
    body_.step(dt, level, kGravity, kMaxFall);
    rescueIfFallen(level);
}

void Enemy::guardEdges(const Level& level) {
    if (!body_.grounded || state_ == EnemyState::Dead) return;
    Vector3 v{body_.velocity.x, 0.0f, body_.velocity.z};
    float speed = Vector3Length(v);
    if (speed < 0.05f) return;
    Vector3 dir = Vector3Scale(v, 1.0f / speed);
    // Vorschau: die Körperbreite plus der Weg der nächsten Zehntelsekunden, auch leicht seitlich
    float look = def_->radius + 0.2f + speed * 0.12f;
    for (float turn : {0.0f, 0.5f, -0.5f}) {
        float c = std::cos(turn), sn = std::sin(turn);
        Vector3 d{dir.x * c - dir.z * sn, 0.0f, dir.x * sn + dir.z * c};
        Vector3 ahead{body_.position.x + d.x * look, body_.position.y, body_.position.z + d.z * look};
        float g = level.groundHeight(ahead, 0.12f, body_.position.y + Level::kStepHeight);
        if (g < body_.position.y - kMaxDrop) {
            // Nur den Anteil Richtung Abgrund wegnehmen: an der Kante entlang rutschen geht weiter
            float along = body_.velocity.x * d.x + body_.velocity.z * d.z;
            if (along > 0.0f) {
                body_.velocity.x -= d.x * along;
                body_.velocity.z -= d.z * along;
            }
        }
    }
}

bool Enemy::leapSafe(const Level& level, float yaw) const {
    float air = 2.0f * def_->leapHeight / kGravity;   // Flugzeit bis zur Absprunghöhe
    Vector3 dir{std::sin(yaw), 0.0f, std::cos(yaw)};
    Vector3 land{body_.position.x + dir.x * def_->leapSpeed * air, body_.position.y, body_.position.z + dir.z * def_->leapSpeed * air};
    // Der Landeplatz braucht festen Boden auf ähnlicher Höhe (auch etwas höher, z. B. eine Stufe)
    float g = level.groundHeight(land, def_->radius * 0.6f, body_.position.y + 1.4f);
    return g >= body_.position.y - kMaxDrop;
}

// Fällt ein Gegner trotzdem (Plattform bröckelt, Stoß aus der Ferne), geht er zum letzten sicheren Punkt zurück,
// nach mehreren Malen zum Startpunkt. Sonst fiele er ewig und der Raum wäre nie "gesäubert".
void Enemy::rescueIfFallen(const Level& level) {
    if (state_ == EnemyState::Dead) return;
    if (body_.grounded) {
        lastSafe_ = body_.position;
        return;
    }
    if (body_.position.y < level.killY + 6.0f || body_.position.y < level.groundY - 25.0f) {
        rescues_++;
        Log::warn(LogCategory::Combat, "{} ist abgestürzt und wird zurückgesetzt ({}. Mal)", def_->name, rescues_);
        body_.position = rescues_ <= 2 ? lastSafe_ : spawnPos_;
        body_.position.y += 0.1f;
        body_.velocity = {0, 0, 0};
        body_.grounded = false;
        state_ = state_ == EnemyState::Spawning ? state_ : EnemyState::Idle;
    }
}

float Enemy::takeHit(float damage, Vector3 knockback) {
    if (state_ == EnemyState::Dead || state_ == EnemyState::Spawning) return 0.0f;
    float dealt = health_.damage(damage);
    flashTimer_ = 0.14f;

    if (health_.dead()) {
        state_ = EnemyState::Dead;
        deadTimer_ = 0.0f;
        body_.velocity.x = knockback.x;
        body_.velocity.z = knockback.z;
        return dealt;
    }

    bool committed = state_ == EnemyState::Telegraph || state_ == EnemyState::Attack;
    bool canStagger = poiseCooldown_ <= 0.0f && (def_->interruptible || !committed);
    if (canStagger) {
        state_ = EnemyState::Hurt;
        timer_ = def_->stagger;
        poiseCooldown_ = def_->stagger + 0.45f;
        body_.velocity.x = knockback.x;
        body_.velocity.z = knockback.z;
    } else {
        // Nicht betäubt: nur ein kleiner Rückstoß, ein getroffener Wartender wird aber aufmerksam
        body_.velocity.x += knockback.x * 0.25f;
        body_.velocity.z += knockback.z * 0.25f;
        if (state_ == EnemyState::Idle) state_ = EnemyState::Chase;
    }
    return dealt;
}

MeleeQuery Enemy::attackQuery() const {
    return {body_.position, yaw_, def_->attackRange + 0.35f, def_->attackHalfAngle};
}

void Enemy::draw(const LitRenderer& r, const Level& level) const {
    const Vector3 p = body_.position;
    const float rad = def_->radius, h = def_->height;
    float shrink = 1.0f;
    if (state_ == EnemyState::Dead) shrink = std::max(0.0f, 1.0f - deadTimer_ / kDeathDuration);
    float rise = 1.0f;  // Auftauchen: von unten aus dem Boden
    if (state_ == EnemyState::Spawning && spawnDuration_ > 0.0f) rise = std::clamp(1.0f - timer_ / spawnDuration_, 0.0f, 1.0f);
    if (shrink <= 0.0f) return;

    float ground = level.groundHeight(p, rad, p.y);
    if (ground > -1000.0f) {
        r.cylinder({p.x, ground + 0.02f, p.z}, rad * 1.2f * shrink * std::max(rise, 0.3f), 0.01f, Color{0, 0, 0, 90});
    }

    const Vector3 fwd{std::sin(yaw_), 0.0f, std::cos(yaw_)};
    const Vector3 side{-fwd.z, 0.0f, fwd.x};
    const Vector3 up{0, 1, 0};
    const float t = (float)GetTime();

    // Art des Gegners aus dem Verhalten und der Größe
    enum class Kind { Imp, Brute, Archer, Leaper };
    Kind kind = Kind::Imp;
    if (def_->behavior == EnemyBehavior::Ranged) kind = Kind::Archer;
    else if (def_->behavior == EnemyBehavior::Leaper) kind = Kind::Leaper;
    else if (rad > 0.6f) kind = Kind::Brute;
    const float s = std::max(0.5f, h / (kind == Kind::Brute ? 2.0f : 1.1f)) * shrink;

    const std::string& id = def_->id;
    auto has = [&](const char* part) { return id.find(part) != std::string::npos; };
    const bool ember = has("ember") || has("cinder") || has("magma");
    const bool storm = has("wind") || has("storm") || has("gale");
    std::string skinName = def_->skin;
    if (skinName.empty()) {
        if (ember) skinName = "ember_skin";
        else if (storm) skinName = "storm_skin";
        else skinName = kind == Kind::Brute ? "brute_hide" : (kind == Kind::Leaper ? "leaper_hide" : "kobold_hide");
    }

    const bool flat = flashTimer_ > 0.0f;  // Trefferblitz: einfarbig weiß
    auto mat = [&](const std::string& name) { return flat ? kNoMaterial : r.material(name); };
    auto tone = [&](Color c) { return flat ? WHITE : c; };
    Color body = flat ? WHITE : def_->color;
    Color dark = lerpColor(def_->color, Color{40, 28, 24, 255}, 0.55f);
    Color horn = lerpColor(def_->color, Color{70, 56, 44, 255}, 0.65f);
    Color belly = lerpColor(def_->color, Color{236, 214, 170, 255}, 0.55f);
    Color boneCol{234, 226, 204, 255};
    Color leather{116, 80, 56, 255};
    Color clothCol = kind == Kind::Archer ? Color{80, 104, 70, 255} : (kind == Kind::Brute ? Color{110, 82, 58, 255} : Color{132, 78, 52, 255});
    if (storm) clothCol = Color{150, 174, 210, 255};
    else if (ember) clothCol = Color{92, 58, 44, 255};
    Color metal{170, 160, 158, 255};
    Color woodCol{150, 120, 96, 255};

    // Haltung erzählt den Zustand
    const float progress = telegraphProgress();
    float atk = 0.0f;   // Fortschritt des Ausfalls
    if (state_ == EnemyState::Attack && def_->behavior != EnemyBehavior::Leaper) atk = std::clamp(1.0f - timer_ / std::max(0.05f, def_->attackActive), 0.0f, 1.0f);
    const bool telegraph = state_ == EnemyState::Telegraph;
    const bool attacking = state_ == EnemyState::Attack;
    const float speed = std::hypot(body_.velocity.x, body_.velocity.z);
    const float moveK = body_.grounded ? std::min(1.0f, speed / std::max(1.0f, def_->chaseSpeed)) : 0.0f;
    float lean = 0.10f * moveK + (kind == Kind::Brute ? 0.22f : 0.0f);
    if (telegraph) lean = -0.32f * progress;
    else if (attacking) lean = 0.42f;
    else if (state_ == EnemyState::Recover) lean = 0.18f;
    else if (state_ == EnemyState::Hurt) lean = -0.42f;
    float hipDrop = 0.0f;
    if (kind == Kind::Leaper && telegraph) hipDrop = 0.16f * s * progress;

    const float sink = (1.0f - rise) * h;
    const Vector3 at{p.x, p.y - sink, p.z};
    r.setAnchor(at, yaw_);

    const float hipBase = (kind == Kind::Brute ? 0.86f : (kind == Kind::Leaper ? 0.30f : 0.40f)) * s;
    const float bob = std::fabs(std::sin(walkPhase_)) * 0.03f * s * moveK + std::sin(t * 2.4f + p.x) * 0.008f * s * (1.0f - moveK);
    const float hip = hipBase - hipDrop + bob;

    auto W = [&](Vector3 l, bool upper) {
        Vector3 v = l;
        if (upper) v = pitchAround(v, {0, hip, 0}, lean);
        return Vector3{at.x + side.x * v.x + fwd.x * v.z, at.y + v.y, at.z + side.z * v.x + fwd.z * v.z};
    };
    // Ein Kegel aus Knochen (Horn, Kralle, Zahn, Stachel) von a nach b
    auto spike = [&](Vector3 a, Vector3 b, float radius, Color c, const char* material) {
        r.useMaterial(mat(material));
        r.taper(a, b, radius, radius * 0.08f, tone(c), false);
        r.clearMaterial();
    };

    // ---- Beine
    const float legLen = (kind == Kind::Brute ? 0.42f : (kind == Kind::Leaper ? 0.30f : 0.20f)) * s;
    const float legR = (kind == Kind::Brute ? 0.15f : (kind == Kind::Leaper ? 0.09f : 0.07f)) * s;
    const bool inAir = !body_.grounded;
    for (float sd : {-1.0f, 1.0f}) {
        float phi = walkPhase_ + (sd > 0.0f ? 0.0f : kPi);
        float stride = (kind == Kind::Brute ? 0.30f : 0.17f) * s * moveK;
        float lift = (kind == Kind::Brute ? 0.14f : 0.10f) * s * moveK;
        float wide = (kind == Kind::Leaper ? 0.24f : (kind == Kind::Brute ? 0.28f : 0.13f)) * s;
        Vector3 foot{sd * wide, 0.05f * s + std::max(0.0f, std::cos(phi)) * lift, std::sin(phi) * stride};
        if (kind == Kind::Leaper) {
            foot.z += -0.12f * s;
            if (telegraph) foot = {sd * wide, 0.05f * s, -0.05f * s};
            if (inAir || attacking) foot = {sd * wide * 0.8f, hip - legLen * 1.7f, -0.55f * s};
        }
        Vector3 hp = W({sd * wide * (kind == Kind::Leaper ? 0.9f : 1.0f), hip - 0.02f * s, kind == Kind::Leaper ? -0.10f * s : 0.0f}, false);
        Vector3 knee, ankle;
        Vector3 bend = kind == Kind::Leaper ? Vector3Add(Vector3Scale(fwd, 0.4f), Vector3Scale(up, 0.9f)) : fwd;
        twoBone(hp, W(foot, false), legLen, legLen, bend, knee, ankle);
        r.useMaterial(mat(skinName));
        r.taper(hp, knee, legR * 1.3f, legR * 0.95f, dark);
        r.taper(knee, ankle, legR * 0.95f, legR * 0.7f, dark);
        r.clearMaterial();
        // Fuß mit Zehen und Krallen
        Vector3 heel = {ankle.x, ankle.y + 0.03f * s, ankle.z};
        r.useMaterial(mat(skinName));
        r.taper(heel, Vector3Add(heel, Vector3Scale(fwd, 0.13f * s)), legR * 1.1f, legR * 0.8f, tone(horn));
        r.clearMaterial();
        for (float toe : {-1.0f, 0.0f, 1.0f}) {
            Vector3 base = Vector3Add(Vector3Add(heel, Vector3Scale(fwd, 0.13f * s)), Vector3Scale(side, toe * legR * 0.5f));
            spike(base, Vector3Add(base, Vector3Add(Vector3Scale(fwd, 0.07f * s), Vector3Scale(side, toe * 0.015f * s))), legR * 0.32f, boneCol, "bone");
        }
        if (kind == Kind::Brute) {   // Fußwickel aus Leder
            r.useMaterial(mat("leather_brown"));
            r.taper(Vector3Add(ankle, {0, 0.03f * s, 0}), Vector3Add(ankle, {0, 0.16f * s, 0}), legR * 0.9f, legR * 1.02f, tone(leather), false);
            r.clearMaterial();
        }
    }

    // ---- Schwanz (Kobolde, Bogenschützen, Springer): pendelt mit dem Schritt
    if (kind != Kind::Brute) {
        float k = kind == Kind::Leaper ? 1.4f : 1.0f;
        float sw1 = std::sin(t * 2.6f + walkPhase_ * 0.5f) * 0.10f * s * (0.4f + moveK);
        float sw2 = std::sin(t * 2.6f + walkPhase_ * 0.5f - 0.9f) * 0.16f * s * (0.4f + moveK);
        Vector3 t0 = W({0, hip + (kind == Kind::Leaper ? 0.12f : 0.0f) * s, -0.20f * s}, false);
        Vector3 t1 = W({sw1, hip - 0.04f * s + (attacking ? 0.16f * s : 0.0f), -0.44f * s * k}, false);
        Vector3 t2 = W({sw2, hip - 0.06f * s + (attacking ? 0.24f * s : 0.0f), -0.66f * s * k}, false);
        Vector3 t3 = W({sw2 * 1.6f, hip - 0.02f * s + (attacking ? 0.32f * s : 0.0f), -0.84f * s * k}, false);
        r.useMaterial(mat(skinName));
        r.taper(t0, t1, 0.075f * s, 0.052f * s, dark);
        r.taper(t1, t2, 0.052f * s, 0.032f * s, dark);
        r.taper(t2, t3, 0.032f * s, 0.006f * s, dark, false);
        r.clearMaterial();
    }

    // ---- Rumpf, Kopf
    Vector3 chest{0, 0, 0}, head{0, 0, 0};
    r.useMaterial(mat(skinName));
    switch (kind) {
        case Kind::Imp:
        case Kind::Archer: {
            float slim = kind == Kind::Archer ? 0.78f : 1.0f;
            chest = W({0, hip + 0.28f * s, 0}, true);
            r.ellipsoid(chest, {0.27f * s * slim, 0.31f * s, 0.24f * s * slim}, body, yaw_);
            r.ellipsoid(W({0, hip + 0.22f * s, 0.07f * s}, true), {0.19f * s * slim, 0.24f * s, 0.19f * s * slim}, tone(belly), yaw_);   // heller Bauch
            head = W({0, hip + 0.70f * s, 0.05f * s}, true);
            r.sphere(head, 0.25f * s, body);
            break;
        }
        case Kind::Brute: {
            chest = W({0, hip + 0.60f * s, 0.05f * s}, true);
            r.ellipsoid(chest, {0.62f * s, 0.60f * s, 0.50f * s}, body, yaw_);
            r.ellipsoid(W({0, hip + 0.25f * s, 0.10f * s}, true), {0.50f * s, 0.32f * s, 0.46f * s}, body, yaw_);   // Wanst
            r.ellipsoid(W({0, hip + 0.28f * s, 0.30f * s}, true), {0.36f * s, 0.26f * s, 0.24f * s}, tone(belly), yaw_);
            head = W({0, hip + 1.05f * s, 0.28f * s}, true);
            r.sphere(head, 0.28f * s, body);
            break;
        }
        case Kind::Leaper: {
            chest = W({0, hip + 0.16f * s, 0}, true);
            r.ellipsoid(chest, {0.34f * s, 0.24f * s, 0.42f * s}, body, yaw_);
            r.ellipsoid(W({0, hip + 0.10f * s, 0.10f * s}, true), {0.26f * s, 0.16f * s, 0.32f * s}, tone(belly), yaw_);
            head = W({0, hip + 0.34f * s, 0.40f * s}, true);
            r.ellipsoid(head, {0.24f * s, 0.19f * s, 0.24f * s}, body, yaw_);
            break;
        }
    }
    r.clearMaterial();

    // ---- Ausrüstung am Rumpf
    if (kind == Kind::Imp || kind == Kind::Archer) {
        // Ledergurt schräg über die Brust, Gürtel und Lendenschurz
        r.useMaterial(mat("leather_brown"));
        r.bar(W({-0.22f * s, hip + 0.52f * s, 0.06f * s}, true), W({0.20f * s, hip + 0.08f * s, 0.10f * s}, true), 0.07f * s, 0.03f * s, tone(leather), up);
        r.cylinder(Vector3Add(W({0, hip - 0.06f * s, 0}, false), {0, 0, 0}), 0.25f * s, 0.07f * s, tone(leather));
        r.clearMaterial();
        r.useMaterial(mat("enemy_cloth"));
        float flap = std::sin(walkPhase_) * 0.05f * s * moveK;
        r.bar(W({0, hip - 0.04f * s, 0.24f * s}, false), W({0, hip - 0.24f * s, 0.26f * s + flap}, false), 0.17f * s, 0.02f * s, tone(clothCol), up);
        r.bar(W({0, hip - 0.04f * s, -0.24f * s}, false), W({0, hip - 0.22f * s, -0.27f * s - flap}, false), 0.17f * s, 0.02f * s, tone(clothCol), up);
        r.clearMaterial();
        r.useMaterial(mat("armor_metal"));
        r.box(W({0, hip - 0.03f * s, 0.255f * s}, false), {0.07f * s, 0.06f * s, 0.02f * s}, tone(metal), yaw_);
        r.clearMaterial();
    } else if (kind == Kind::Brute) {
        r.useMaterial(mat("enemy_cloth"));
        r.taper(W({0, hip - 0.30f * s, 0}, false), W({0, hip + 0.06f * s, 0}, false), 0.46f * s, 0.40f * s, tone(clothCol), false);
        r.clearMaterial();
        r.useMaterial(mat("leather_brown"));
        r.cylinder(W({0, hip + 0.04f * s, 0}, false), 0.42f * s, 0.10f * s, tone(leather));
        r.clearMaterial();
        r.useMaterial(mat("armor_metal"));
        r.box(W({0, hip + 0.09f * s, 0.42f * s}, false), {0.24f * s, 0.16f * s, 0.05f * s}, tone(metal), yaw_);
        r.clearMaterial();
    } else {
        // Springer: Rückenstacheln in einer Reihe, Warzen
        for (int i = 0; i < 5; i++) {
            float z = (0.28f - 0.14f * (float)i) * s;
            Vector3 b0 = W({0, hip + (0.30f - 0.03f * (float)i) * s, z}, true);
            spike(b0, Vector3Add(b0, {0, (0.16f - 0.015f * (float)i) * s, 0}), 0.045f * s, boneCol, "bone");
        }
    }

    // Glut-Adern bei Ember-Gegnern, kreisende Windkugeln bei Sturmgegnern
    if (ember && !flat) {
        for (int i = 0; i < 6; i++) {
            float a = 1.7f * (float)i + 0.6f;
            float glow = 0.6f + 0.4f * std::sin(t * 3.0f + (float)i);
            Vector3 c = W({std::cos(a) * 0.26f * s, hip + (0.14f + 0.07f * (float)i) * s, std::sin(a) * 0.24f * s}, true);
            r.sphere(c, (0.04f + 0.02f * glow) * s, Color{255, (unsigned char)(120 + 50 * glow), 40, 244});
        }
    } else if (storm && !flat) {
        for (int i = 0; i < 2; i++) {
            float a = t * 2.4f + kPi * (float)i;
            r.sphere({at.x + std::cos(a) * rad * 1.5f, at.y + h * (0.55f + 0.12f * std::sin(a * 2.0f)), at.z + std::sin(a) * rad * 1.5f}, 0.07f * s, Color{170, 230, 255, 244});
        }
    }

    // ---- Gesicht: Schnauze, Kiefer, Zähne, Augen, Brauen, Hörner und Ohren
    if (state_ != EnemyState::Dead) {
        bool alert = telegraph || attacking;
        Color eyeCol = alert ? Color{255, 230, 90, 244} : (storm ? Color{150, 230, 255, 244} : (ember ? Color{255, 150, 60, 244} : Color{240, 200, 80, 244}));
        float eyeR = (kind == Kind::Leaper ? 0.075f : 0.06f) * s;
        const float jaw = alert ? 0.045f * s : 0.0f;   // der Kiefer klappt beim Angriff auf
        if (kind != Kind::Leaper) {
            Vector3 muzzle = Vector3Add(head, Vector3Add(Vector3Scale(fwd, (kind == Kind::Brute ? 0.20f : 0.20f) * s), {0, -0.04f * s, 0}));
            r.useMaterial(mat(skinName));
            r.ellipsoid(muzzle, {0.13f * s, 0.10f * s, 0.15f * s}, body, yaw_);
            r.ellipsoid(Vector3Add(muzzle, Vector3Add(Vector3Scale(fwd, 0.01f * s), {0, -0.09f * s - jaw, 0})), {0.11f * s, 0.05f * s, 0.13f * s}, tone(dark), yaw_);   // Unterkiefer
            r.clearMaterial();
            for (float sd : {-1.0f, 1.0f}) {
                r.sphere(Vector3Add(muzzle, Vector3Add(Vector3Scale(fwd, 0.14f * s), Vector3Add(Vector3Scale(side, sd * 0.045f * s), {0, 0.03f * s, 0}))), 0.018f * s, tone(Color{30, 20, 20, 255}));   // Nasenlöcher
                Vector3 f0 = Vector3Add(muzzle, Vector3Add(Vector3Scale(fwd, 0.10f * s), Vector3Add(Vector3Scale(side, sd * 0.06f * s), {0, -0.07f * s - jaw, 0})));
                spike(f0, Vector3Add(f0, {0, (kind == Kind::Brute ? 0.13f : 0.09f) * s, 0}), (kind == Kind::Brute ? 0.028f : 0.02f) * s, boneCol, "bone");   // Hauer und Zähne
            }
        }
        for (float sd : {-1.0f, 1.0f}) {
            Vector3 eye = kind == Kind::Leaper ? Vector3Add(head, {side.x * sd * 0.10f * s, 0.15f * s, side.z * sd * 0.10f * s})
                                               : Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.22f * s), Vector3Add(Vector3Scale(side, sd * 0.10f * s), {0, 0.05f * s, 0})));
            if (kind == Kind::Brute) eye = Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.22f * s), Vector3Add(Vector3Scale(side, sd * 0.11f * s), {0, 0.06f * s, 0})));
            r.sphere(eye, eyeR, eyeCol);
            r.sphere(Vector3Add(eye, Vector3Scale(fwd, eyeR * 0.75f)), eyeR * 0.42f, tone(Color{20, 14, 14, 255}));   // Pupille
            // Schwere Braue über dem Auge
            r.bar(Vector3Add(eye, Vector3Add(Vector3Scale(side, -sd * eyeR * 1.3f), {0, eyeR * 1.3f, 0})),
                  Vector3Add(eye, Vector3Add(Vector3Scale(side, sd * eyeR * 1.2f), {0, eyeR * 0.75f, 0})), eyeR * 0.6f, eyeR * 0.55f, tone(dark), up);
            if (kind != Kind::Leaper) {
                // Hörner: zwei Stücke, leicht nach hinten gebogen
                Vector3 hb = Vector3Add(head, Vector3Add(Vector3Scale(side, sd * (kind == Kind::Brute ? 0.14f : 0.13f) * s), {0, 0.20f * s, -0.02f * s}));
                Vector3 hm = Vector3Add(hb, Vector3Add(Vector3Scale(side, sd * 0.06f * s), {0, (kind == Kind::Brute ? 0.10f : 0.14f) * s, -0.02f * s}));
                Vector3 ht = Vector3Add(hm, Vector3Add(Vector3Scale(side, sd * 0.02f * s), {0, (kind == Kind::Brute ? 0.10f : 0.15f) * s, 0.05f * s}));
                r.useMaterial(mat("bone"));
                r.taper(hb, hm, 0.05f * s, 0.035f * s, tone(horn));
                r.taper(hm, ht, 0.035f * s, 0.006f * s, tone(horn), false);
                r.clearMaterial();
            }
            if (kind == Kind::Imp || kind == Kind::Archer) {
                // Spitze, abstehende Ohren
                Vector3 eb = Vector3Add(head, Vector3Scale(side, sd * 0.24f * s));
                Vector3 et = Vector3Add(eb, Vector3Add(Vector3Scale(side, sd * 0.30f * s), Vector3Add({0, 0.10f * s, 0}, Vector3Scale(fwd, -0.06f * s))));
                r.useMaterial(mat(skinName));
                r.taper(eb, et, 0.075f * s, 0.012f * s, body, false);
                r.clearMaterial();
            }
            if (kind == Kind::Leaper) {
                // Augenlider und Warzen
                r.useMaterial(mat(skinName));
                r.ellipsoid(Vector3Add(eye, {0, eyeR * 0.5f, 0}), {eyeR * 1.2f, eyeR * 0.5f, eyeR * 1.2f}, body, yaw_);
                r.clearMaterial();
            }
        }
        if (kind == Kind::Leaper) {
            r.bar(Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.23f * s), Vector3Add(Vector3Scale(side, -0.13f * s), {0, -0.04f * s, 0}))),
                  Vector3Add(head, Vector3Add(Vector3Scale(fwd, 0.23f * s), Vector3Add(Vector3Scale(side, 0.13f * s), {0, -0.04f * s, 0}))), 0.03f * s, 0.03f * s, tone(Color{50, 26, 26, 255}), up);   // breites Maul
            for (int i = 0; i < 4; i++) {
                float a = 2.1f * (float)i + 0.4f;
                r.sphere(W({std::cos(a) * 0.18f * s, hip + (0.22f + 0.02f * (float)i) * s, std::sin(a) * 0.22f * s}, true), 0.03f * s, tone(dark));
            }
        }
        if (kind == Kind::Archer) {
            // Kapuze: Stoff über Kopf und Nacken, mit Zipfel
            r.useMaterial(mat("enemy_cloth"));
            r.ellipsoid(Vector3Add(head, Vector3Add(Vector3Scale(fwd, -0.09f * s), {0, 0.07f * s, 0})), {0.26f * s, 0.23f * s, 0.25f * s}, tone(clothCol), yaw_);
            Vector3 hb = Vector3Add(head, Vector3Add(Vector3Scale(fwd, -0.15f * s), {0, 0.18f * s, 0}));
            r.taper(hb, Vector3Add(hb, Vector3Add(Vector3Scale(fwd, -0.16f * s), {0, 0.10f * s, 0})), 0.10f * s, 0.01f * s, tone(clothCol), false);
            r.taper(W({0, hip + 0.50f * s, 0}, true), W({0, hip + 0.60f * s, 0}, true), 0.24f * s, 0.20f * s, tone(clothCol), false);   // Schulterkragen
            r.clearMaterial();
        }
    }

    // ---- Arme und Waffen
    if (state_ != EnemyState::Dead) {
        const float armLen = (kind == Kind::Brute ? 0.46f : (kind == Kind::Leaper ? 0.16f : 0.20f)) * s;
        const float armR = (kind == Kind::Brute ? 0.15f : 0.06f) * s;
        const float shY = kind == Kind::Brute ? 0.85f : (kind == Kind::Leaper ? 0.22f : 0.44f);
        const float shX = kind == Kind::Brute ? 0.66f : (kind == Kind::Leaper ? 0.22f : 0.26f);
        for (float sd : {-1.0f, 1.0f}) {
            Vector3 sh = W({sd * shX * s, hip + shY * s, kind == Kind::Leaper ? 0.20f * s : 0.0f}, true);
            bool weaponArm = sd > 0.0f && kind != Kind::Leaper;
            bool bowArm = kind == Kind::Archer && sd < 0.0f;
            Vector3 hand;
            Vector3 wdir{0, -0.4f, 0.9f};   // Richtung der Waffe
            if (kind == Kind::Leaper) {
                hand = W({sd * 0.20f * s, 0.06f * s, 0.34f * s}, false);
                if (attacking) hand = W({sd * 0.20f * s, hip + 0.16f * s, 0.50f * s}, false);
            } else if (bowArm) {
                hand = W({0.02f * s, hip + 0.52f * s, 0.46f * s}, true);
            } else if (weaponArm && kind == Kind::Archer) {
                float pull = telegraph ? progress : 0.0f;
                hand = W({0.05f * s, hip + 0.50f * s, (0.46f - 0.06f - 0.28f * pull) * s}, true);
            } else if (weaponArm) {
                // Nahkämpfer: Waffe hebt sich in der Vorwarnung über den Kopf und schlägt beim Ausfall nach vorn unten
                float reach = kind == Kind::Brute ? 1.0f : 0.8f;
                Vector3 idle{0.30f * s, hip + 0.34f * s, 0.30f * s * reach};
                Vector3 raised{0.22f * s, hip + (kind == Kind::Brute ? 1.5f : 0.98f) * s, -0.16f * s};
                Vector3 slam{0.10f * s, hip + 0.22f * s, 0.62f * s * reach};
                Vector3 hl = idle;
                Vector3 wd{0.2f, -0.3f, 0.9f};
                if (telegraph) {
                    hl = Vector3Lerp(idle, raised, progress);
                    wd = Vector3Lerp(wd, Vector3{0.0f, 0.95f, -0.35f}, progress);
                } else if (attacking) {
                    hl = Vector3Lerp(raised, slam, atk);
                    wd = Vector3Lerp({0.0f, 0.95f, -0.35f}, {0.0f, -0.55f, 0.85f}, atk);
                } else if (state_ == EnemyState::Recover) {
                    hl = slam;
                    wd = {0.0f, -0.6f, 0.8f};
                }
                hand = W(hl, true);
                wdir = wd;
            } else {
                float phiA = walkPhase_ + (sd > 0.0f ? kPi : 0.0f);
                hand = W({sd * (shX + 0.05f) * s, hip + (shY - 0.34f) * s, -std::sin(phiA) * 0.16f * s * moveK + 0.03f * s}, true);
            }
            Vector3 elbow, wrist;
            twoBone(sh, hand, armLen, armLen, Vector3Add(Vector3Scale(side, sd * 0.7f), {0, -0.3f, -0.2f}), elbow, wrist);
            r.useMaterial(mat(skinName));
            r.taper(sh, elbow, armR * 1.15f, armR * 0.9f, body);
            r.taper(elbow, wrist, armR * 0.9f, armR * 0.75f, body);
            r.clearMaterial();
            if (kind == Kind::Brute) {
                // Schulterpanzer aus Metall mit Stacheln, Armband aus Leder
                r.useMaterial(mat("armor_metal"));
                r.ellipsoid(Vector3Add(sh, {0, 0.06f * s, 0}), {0.24f * s, 0.15f * s, 0.24f * s}, tone(metal), yaw_);
                r.clearMaterial();
                for (int i = 0; i < 3; i++) {
                    float a = 0.9f * (float)(i - 1);
                    Vector3 b0 = Vector3Add(sh, Vector3Add(Vector3Scale(side, sd * (0.10f + 0.06f * std::cos(a)) * s), Vector3Add({0, 0.14f * s, 0}, Vector3Scale(fwd, std::sin(a) * 0.14f * s))));
                    spike(b0, Vector3Add(b0, Vector3Add(Vector3Scale(side, sd * 0.06f * s), {0, 0.14f * s, 0})), 0.04f * s, metal, "armor_metal");
                }
                r.useMaterial(mat("leather_brown"));
                r.taper(elbow, Vector3Add(elbow, Vector3Scale(Vector3Subtract(wrist, elbow), 0.55f)), armR * 0.98f, armR * 0.92f, tone(leather), false);
                r.clearMaterial();
            } else if (kind != Kind::Leaper) {
                r.useMaterial(mat("leather_brown"));
                r.taper(elbow, Vector3Add(elbow, Vector3Scale(Vector3Subtract(wrist, elbow), 0.5f)), armR * 0.95f, armR * 0.85f, tone(leather), false);   // Armschutz
                r.clearMaterial();
            }
            r.useMaterial(mat(skinName));
            r.sphere(wrist, armR * 1.05f, tone(dark));
            r.clearMaterial();
            // Krallen an den Fingern
            Vector3 clawDir = Vector3Normalize(Vector3Add(Vector3Subtract(wrist, elbow), Vector3Scale(fwd, armLen * 0.4f)));
            for (float fg : {-1.0f, 0.0f, 1.0f}) {
                Vector3 base = Vector3Add(wrist, Vector3Scale(side, fg * armR * 0.55f));
                spike(base, Vector3Add(base, Vector3Scale(clawDir, armR * 1.9f)), armR * 0.3f, boneCol, "bone");
            }

            if (weaponArm && kind == Kind::Brute) {
                Vector3 d = Vector3Normalize({fwd.x * wdir.z + side.x * wdir.x, wdir.y, fwd.z * wdir.z + side.z * wdir.x});
                Vector3 tip = Vector3Add(wrist, Vector3Scale(d, 1.05f * s));
                Vector3 perp = Vector3Normalize(Vector3CrossProduct(up, d));
                Vector3 perp2 = Vector3CrossProduct(d, perp);
                r.useMaterial(mat("club_wood"));
                r.taper(Vector3Subtract(wrist, Vector3Scale(d, 0.15f * s)), tip, 0.055f * s, 0.10f * s, tone(woodCol));
                r.clearMaterial();
                r.useMaterial(mat("armor_metal"));
                r.bar(Vector3Subtract(tip, Vector3Scale(d, 0.10f * s)), Vector3Add(tip, Vector3Scale(d, 0.30f * s)), 0.30f * s, 0.30f * s, tone(metal), up);
                r.clearMaterial();
                for (int i = 0; i < 6; i++) {
                    float a = 1.0472f * (float)i;
                    Vector3 out = Vector3Add(Vector3Scale(perp, std::cos(a)), Vector3Scale(perp2, std::sin(a)));
                    Vector3 b0 = Vector3Add(Vector3Add(tip, Vector3Scale(d, 0.08f * s)), Vector3Scale(out, 0.15f * s));
                    spike(b0, Vector3Add(b0, Vector3Scale(out, 0.13f * s)), 0.035f * s, metal, "armor_metal");
                }
                spike(Vector3Add(tip, Vector3Scale(d, 0.30f * s)), Vector3Add(tip, Vector3Scale(d, 0.44f * s)), 0.05f * s, metal, "armor_metal");
            } else if (weaponArm && kind == Kind::Imp) {
                Vector3 d = Vector3Normalize({fwd.x * wdir.z + side.x * wdir.x, wdir.y, fwd.z * wdir.z + side.z * wdir.x});
                Vector3 tip = Vector3Add(wrist, Vector3Scale(d, 0.52f * s));
                Vector3 perp = Vector3Normalize(Vector3CrossProduct(up, d));
                Vector3 perp2 = Vector3CrossProduct(d, perp);
                r.useMaterial(mat("club_wood"));
                r.taper(Vector3Subtract(wrist, Vector3Scale(d, 0.06f * s)), tip, 0.04f * s, 0.085f * s, tone(woodCol));
                r.clearMaterial();
                for (int i = 0; i < 4; i++) {
                    float a = 1.5708f * (float)i + 0.4f;
                    Vector3 out = Vector3Add(Vector3Scale(perp, std::cos(a)), Vector3Scale(perp2, std::sin(a)));
                    Vector3 b0 = Vector3Add(Vector3Subtract(tip, Vector3Scale(d, 0.05f * s)), Vector3Scale(out, 0.075f * s));
                    spike(b0, Vector3Add(b0, Vector3Scale(out, 0.07f * s)), 0.022f * s, metal, "armor_metal");
                }
            }
            if (bowArm) {
                // Bogen: gebogener Holzstab mit Sehne; leuchtet beim Zielen, der Pfeil liegt an
                Color bowCol = telegraph ? Color{255, 210, 90, 255} : Color{150, 110, 76, 255};
                Vector3 top = Vector3Add(wrist, {0, 0.40f * s, -0.05f * s});
                Vector3 bot = Vector3Add(wrist, {0, -0.40f * s, -0.05f * s});
                Vector3 topO = Vector3Add(wrist, Vector3Add({0, 0.20f * s, 0}, Vector3Scale(fwd, 0.10f * s)));
                Vector3 botO = Vector3Add(wrist, Vector3Add({0, -0.20f * s, 0}, Vector3Scale(fwd, 0.10f * s)));
                r.useMaterial(mat("club_wood"));
                r.taper(top, topO, 0.02f * s, 0.032f * s, tone(bowCol));
                r.taper(topO, botO, 0.035f * s, 0.035f * s, tone(bowCol));
                r.taper(botO, bot, 0.032f * s, 0.02f * s, tone(bowCol));
                r.clearMaterial();
                float pull = telegraph ? progress : 0.0f;
                Vector3 nock = Vector3Subtract(wrist, Vector3Scale(fwd, (0.05f + 0.28f * pull) * s));
                r.limb(top, nock, 0.008f * s, Color{230, 226, 210, 255});
                r.limb(bot, nock, 0.008f * s, Color{230, 226, 210, 255});
                if (telegraph) {
                    r.limb(nock, Vector3Add(wrist, Vector3Scale(fwd, 0.28f * s)), 0.012f * s, Color{255, 220, 120, 244});
                }
            }
        }
        if (kind == Kind::Archer) {
            // Köcher auf dem Rücken mit Pfeilen
            Vector3 q0 = W({-0.10f * s, hip + 0.62f * s, -0.24f * s}, true), q1 = W({0.10f * s, hip + 0.16f * s, -0.28f * s}, true);
            r.useMaterial(mat("leather_brown"));
            r.taper(q0, q1, 0.075f * s, 0.06f * s, tone(leather));
            r.clearMaterial();
            for (int i = 0; i < 3; i++) {
                Vector3 a0 = Vector3Add(q0, Vector3Add(Vector3Scale(side, (0.03f * (float)i - 0.03f) * s), {0, 0.0f, 0}));
                Vector3 a1 = Vector3Add(a0, Vector3Add({0, 0.20f * s, 0}, Vector3Scale(side, (0.02f * (float)i - 0.02f) * s)));
                r.limb(a0, a1, 0.008f * s, Color{170, 140, 100, 255});
                r.sphere(a1, 0.028f * s, tone(Color{210, 96, 84, 255}));   // Federn
            }
        }
    }
    r.clearAnchor();

    // Aura: warnt vor dem Angriff (gelbes Pulsieren)
    if (telegraph && !flat) {
        float pulse = 0.5f + 0.5f * std::sin(t * 28.0f);
        r.sphere({at.x, at.y + h * 0.5f, at.z}, rad * 1.25f, Color{255, 226, 90, (unsigned char)(10 + 34 * pulse * progress)});
    }
}

}  // namespace aldoria
