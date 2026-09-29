#include "entities/enemy_def.h"

#include <algorithm>

#include "core/file_util.h"
#include "core/log.h"

namespace aldoria {

EnemyDef EnemyDef::fromJson(const json& j) {
    EnemyDef d;
    if (!j.is_object()) return d;
    d.id = j.value("id", d.id);
    d.name = j.value("name", d.name);
    d.skin = j.value("skin", d.skin);
    std::string behavior = j.value("behavior", std::string("melee"));
    if (behavior == "ranged") d.behavior = EnemyBehavior::Ranged;
    else if (behavior == "leaper") d.behavior = EnemyBehavior::Leaper;
    else if (behavior != "melee") Log::warn(LogCategory::Loading, "Gegner '{}': unbekanntes Verhalten '{}'", d.id, behavior);

    d.maxHealth = j.value("max_health", d.maxHealth);
    d.radius = j.value("radius", d.radius);
    d.height = j.value("height", d.height);
    d.chaseSpeed = j.value("chase_speed", d.chaseSpeed);
    d.sightRange = j.value("sight_range", d.sightRange);
    d.loseRange = j.value("lose_range", d.loseRange);
    d.attackRange = j.value("attack_range", d.attackRange);
    d.telegraph = j.value("telegraph", d.telegraph);
    d.attackActive = j.value("attack_active", d.attackActive);
    d.attackRecovery = j.value("attack_recovery", d.attackRecovery);
    d.attackLungeSpeed = j.value("attack_lunge_speed", d.attackLungeSpeed);
    d.attackHalfAngle = j.value("attack_half_angle_deg", d.attackHalfAngle * 57.2957795f) * 0.0174532925f;
    d.damage = j.value("damage", d.damage);
    d.knockback = j.value("knockback", d.knockback);
    d.stagger = j.value("stagger", d.stagger);
    d.interruptible = j.value("interruptible", d.interruptible);
    d.spawnTime = j.value("spawn_time", d.spawnTime);
    d.preferredMin = j.value("preferred_min", d.preferredMin);
    d.preferredMax = j.value("preferred_max", d.preferredMax);
    d.shootCooldown = j.value("shoot_cooldown", d.shootCooldown);
    d.projectileSpeed = j.value("projectile_speed", d.projectileSpeed);
    d.leapRange = j.value("leap_range", d.leapRange);
    d.leapSpeed = j.value("leap_speed", d.leapSpeed);
    d.leapHeight = j.value("leap_height", d.leapHeight);
    d.leapCooldown = j.value("leap_cooldown", d.leapCooldown);
    d.healthDropChance = j.value("health_drop_chance", d.healthDropChance);
    if (auto it = j.find("color"); it != j.end() && it->is_array() && it->size() >= 3) {
        auto c = [&](size_t i) { return (unsigned char)std::clamp((*it)[i].get<int>(), 0, 255); };
        d.color = {c(0), c(1), c(2), 255};
    }
    return d;
}

std::optional<EnemyDef> EnemyDef::loadFromFile(const std::string& path) {
    std::string error;
    auto j = loadJsonFile(path, error);
    if (!j) {
        Log::error(LogCategory::Loading, "Gegner konnte nicht geladen werden: {}", error);
        return std::nullopt;
    }
    return fromJson(*j);
}

}  // namespace aldoria
