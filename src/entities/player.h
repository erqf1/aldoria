#pragma once

#include <array>
#include <optional>
#include <string>

#include "combat/melee.h"
#include "combat/player_modifiers.h"
#include "combat/weapon_def.h"
#include "components/health.h"
#include "core/event_bus.h"
#include "core/input.h"
#include "core/json.h"
#include "raylib.h"
#include "render/lit_renderer.h"
#include "world/kinematic_body.h"
#include "world/level.h"

namespace aldoria {

// Alle Werte kommen aus data/config/player.json; die Vorgaben hier gelten für fehlende Felder.
struct PlayerConfig {
    // Bewegung
    float walkSpeed = 5.5f;
    float sprintSpeed = 8.5f;
    float groundAccel = 50.0f;
    float groundDecel = 60.0f;
    float airAccel = 18.0f;
    float turnRate = 14.0f;
    float radius = 0.4f;
    float height = 1.8f;
    // Sprung
    float jumpSpeed = 9.0f;
    float gravity = 28.0f;
    float maxFallSpeed = 40.0f;
    float coyoteTime = 0.10f;   // Sprung ist noch kurz nach dem Verlassen einer Kante möglich
    float jumpBuffer = 0.12f;   // Sprung wird kurz vor der Landung schon vorgemerkt
    float jumpCut = 0.5f;       // Taste früh loslassen kürzt den Sprung
    // Kampf
    float maxHealth = 100.0f;
    float attackBuffer = 0.22f;
    float dodgeBuffer = 0.15f;
    float dodgeSpeed = 10.0f;
    float dodgeDuration = 0.40f;
    float dodgeIFrames = 0.26f;
    float dodgeCooldown = 0.25f;
    float hurtTime = 0.28f;
    float hurtInvulnerability = 0.9f;
    // Dash (Fähigkeit, im Dungeon zu finden)
    float dashSpeed = 20.0f;
    float dashDuration = 0.18f;
    float dashCooldown = 0.7f;
    float dashBuffer = 0.12f;
    float dashIFrames = 0.1f;
    // Funkenwurf (Rechtsklick)
    float sparkCooldown = 2.2f;
    float sparkDamage = 14.0f;
    float sparkSpeed = 24.0f;
    // Heiltrank
    float flaskHeal = 45.0f;

    static PlayerConfig fromJson(const json& j);
    static PlayerConfig loadFromFile(const std::string& path);
};

// Abbild der Eingabe für einen Frame. So lässt sich der Spieler ohne Fenster und Tastatur testen.
struct PlayerInput {
    Vector2 move{0, 0};  // x = rechts, y = vorwärts
    bool jumpPressed = false;
    bool jumpDown = false;
    bool sprint = false;
    bool attackPressed = false;
    bool dodgePressed = false;
    bool dashPressed = false;
    bool secondaryPressed = false;
};

inline PlayerInput makePlayerInput(const Input& in, bool enabled) {
    PlayerInput p;
    if (!enabled) return p;
    p.move = in.moveAxis();
    p.jumpPressed = in.pressed(Action::Jump);
    p.jumpDown = in.down(Action::Jump);
    p.sprint = in.down(Action::Sprint);
    p.attackPressed = in.pressed(Action::Attack);
    p.dodgePressed = in.pressed(Action::Dodge);
    p.dashPressed = in.pressed(Action::Dash);
    p.secondaryPressed = in.pressed(Action::Secondary);
    return p;
}

struct PlayerFrame {
    const Level* level = nullptr;
    float cameraYaw = 0.0f;                // Bewegung ist kamerarelativ
    std::optional<Vector3> assistTarget;   // nächster Gegner in Reichweite: Angriffe zielen leicht darauf
};

enum class PlayerState { Normal, Attacking, Dodging, Dashing, Hurt, Dead };
enum class AttackPhase { Windup, Active, Recovery };

const char* playerStateName(PlayerState s);

class Player {
public:
    void configure(const PlayerConfig& config);
    void setWeapon(WeaponDef weapon);
    void spawn(Vector3 position);
    void respawn() { spawn(spawnPoint_); }
    // Setzt die Figur um (Raumwechsel), ohne Leben oder Zustand des Spielstands anzutasten
    void teleport(Vector3 position, float yaw);

    // Upgrades und Fähigkeiten
    void setModifiers(const PlayerModifiers& mods);
    void setBonusMaxHealth(float bonus);
    void setDashUnlocked(bool unlocked) { dashUnlocked_ = unlocked; }
    bool dashUnlocked() const { return dashUnlocked_; }
    void setDoubleJumpUnlocked(bool unlocked) { doubleJumpUnlocked_ = unlocked; }
    bool doubleJumpUnlocked() const { return doubleJumpUnlocked_; }
    bool airJumpAvailable() const { return doubleJumpUnlocked_ && !airJumpUsed_; }
    const PlayerModifiers& modifiers() const { return mods_; }

    void update(float dt, const PlayerInput& input, const PlayerFrame& frame, EventBus& events);
    void draw(const LitRenderer& renderer, const Level& level) const;

    // Kampf
    bool attackActive() const { return state_ == PlayerState::Attacking && phase_ == AttackPhase::Active; }
    const AttackDef& currentAttack() const { return weapon_.attacks[comboStep_]; }
    MeleeQuery attackQuery() const;
    int swingId() const { return swingId_; }
    bool canBeHit() const;
    // Gibt den tatsächlichen Schaden zurück (0, wenn der Treffer ins Leere ging)
    float takeDamage(float amount, Vector3 knockbackVelocity, EventBus& events);
    float healBy(float amount, EventBus& events);
    // Wurde ein Funkenwurf ausgelöst? Liefert Startpunkt und Richtung einmalig.
    bool consumeCast(Vector3& origin, Vector3& direction);
    float sparkReadyFraction() const;
    float dashReadyFraction() const;

    Vector3 position() const { return body_.position; }
    Vector3 velocity() const { return body_.velocity; }
    // Umhang für Tests: Punkte der Stoffsimulation und wie tief der Stoff in Rumpf, Rock, Schultern oder Beine ragt (0 = nirgends)
    const std::array<Vector3, kClothCols * kClothRows>& capeGrid() const { return capePos_; }
    float capePenetration() const;
    KinematicBody& body() { return body_; }
    const KinematicBody& body() const { return body_; }
    bool grounded() const { return body_.grounded; }
    float yaw() const { return yaw_; }
    PlayerState state() const { return state_; }
    int comboStep() const { return comboStep_; }
    bool dead() const { return state_ == PlayerState::Dead; }
    const Health& health() const { return health_; }
    const WeaponDef& weapon() const { return weapon_; }
    const PlayerConfig& config() const { return cfg_; }
    // Punkt, auf den die Kamera schaut (etwa Brusthöhe)
    Vector3 focusPoint() const {
        return {body_.position.x, body_.position.y + cfg_.height * 0.8f, body_.position.z};
    }

    bool godMode = false;  // nur für Tests und Entwicklung

private:
    struct AttackTimes {
        float windup, active, recovery;
        float total() const { return windup + active + recovery; }
    };
    AttackTimes attackTimes(const AttackDef& a) const;
    void refreshMaxHealth();

    void updateNormal(float dt, const PlayerInput& in, const PlayerFrame& frame, Vector3 wish, EventBus& events);
    void updateAttacking(float dt, const PlayerInput& in, const PlayerFrame& frame, Vector3 wish, EventBus& events);
    void updateDodging(float dt);
    void updateDashing(float dt);
    void updateHurt(float dt);
    void locomotion(float dt, const PlayerInput& in, Vector3 wish, float speedScale, bool allowTurn);
    void doJump(EventBus& events);
    void doAirJump(EventBus& events);
    void startAttack(int step, const PlayerFrame& frame, Vector3 wish, EventBus& events);
    void endAttack();
    void startDodge(Vector3 wish, EventBus& events);
    bool tryDash(Vector3 wish, EventBus& events);
    void tryCast(const PlayerFrame& frame, EventBus& events);

    PlayerConfig cfg_;
    PlayerModifiers mods_;
    WeaponDef weapon_;
    KinematicBody body_;
    Health health_{100.0f};
    PlayerState state_ = PlayerState::Normal;
    AttackPhase phase_ = AttackPhase::Windup;

    Vector3 spawnPoint_{0, 0, 0};
    float yaw_ = 3.14159265f;  // blickt zunächst in -Z
    float stateTimer_ = 0.0f;
    float bonusMaxHealth_ = 0.0f;

    // Sprung
    float coyote_ = 0.0f;
    float jumpBuffer_ = 0.0f;
    bool jumping_ = false;

    // Kampf
    int comboStep_ = 0;
    float comboTimer_ = 0.0f;
    float attackBuffer_ = 0.0f;
    float dodgeBuffer_ = 0.0f;
    float dodgeCooldown_ = 0.0f;
    Vector3 dodgeDir_{0, 0, 1};
    int swingId_ = 0;

    // Doppelsprung
    bool doubleJumpUnlocked_ = false;
    bool airJumpUsed_ = false;

    // Umhang: Stoffsimulation (Verlet), Punkte in Weltkoordinaten
    std::array<Vector3, kClothCols * kClothRows> capePos_{}, capePrev_{}, capeDraw_{};   // capeDraw_ = Stoff mit Faltenprofil
    bool capeInit_ = false;
    float capeTime_ = 0.0f;
    float capeDtPrev_ = 1.0f / 60.0f;
    float landDip_ = 0.0f;    // kurzes Einfedern nach der Landung
    float deadAnim_ = 0.0f;   // Zeit seit dem Tod (Sturzanimation)
    float pose_ = 1.0f;       // 1 aufrecht, geduckt beim Ausweichen (weich übergeblendet)

    // Körperhaltung, die Zeichnen und Umhang teilen
    struct RigFrame {
        float hipY = 0.88f, lean = 0.0f, tumble = 0.0f, fallA = 0.0f, pose = 1.0f, moveK = 0.0f;
        Vector3 fwd{0, 0, 1}, side{1, 0, 0};
    };
    RigFrame rigFrame() const;
    Vector3 rigPoint(const RigFrame& f, Vector3 at, Vector3 local, bool upper) const;
    Vector3 footTarget(const RigFrame& f, float s) const;
    Vector3 capeRestLocal(const RigFrame& f, int c, int r) const;
    // Kapseln, an denen der Umhang abprallt (Rumpf, Rock, Schultern, Beine)
    struct CapeColliders {
        Vector3 torsoA, torsoB, skirtA, skirtB, shoulderL, shoulderR, legTop[2], legFoot[2];
    };
    CapeColliders capeColliders(const RigFrame& f, Vector3 at) const;
    void simulateCape(float dt);
    float leanAngle() const;

    // Dash
    bool dashUnlocked_ = false;
    float dashBuffer_ = 0.0f;
    float dashCooldown_ = 0.0f;
    bool airDashUsed_ = false;
    Vector3 dashDir_{0, 0, 1};

    // Funkenwurf
    float castBuffer_ = 0.0f;
    float sparkCooldown_ = 0.0f;
    float castGlow_ = 0.0f;
    float animPhase_ = 0.0f;  // Laufanimation
    bool castRequested_ = false;
    Vector3 castOrigin_{0, 0, 0};
    Vector3 castDirection_{0, 0, 1};
};

}  // namespace aldoria
