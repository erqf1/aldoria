#include "combat/weapon_def.h"

#include "core/file_util.h"
#include "core/log.h"
#include "raylib.h"

namespace aldoria {
namespace {

AttackDef attackFromJson(const json& j) {
    AttackDef a;
    a.name = j.value("name", a.name);
    a.damage = j.value("damage", a.damage);
    a.windup = j.value("windup", a.windup);
    a.active = j.value("active", a.active);
    a.recovery = j.value("recovery", a.recovery);
    a.range = j.value("range", a.range);
    a.halfAngle = j.value("half_angle_deg", a.halfAngle * RAD2DEG) * DEG2RAD;
    a.lunge = j.value("lunge", a.lunge);
    a.knockback = j.value("knockback", a.knockback);
    a.hitstop = j.value("hitstop", a.hitstop);
    a.sweepFrom = j.value("sweep_from_deg", a.sweepFrom * RAD2DEG) * DEG2RAD;
    a.sweepTo = j.value("sweep_to_deg", a.sweepTo * RAD2DEG) * DEG2RAD;
    a.moveScale = j.value("move_scale", a.moveScale);
    return a;
}

}  // namespace

WeaponDef WeaponDef::fromJson(const json& j) {
    WeaponDef w;
    if (!j.is_object()) return w;
    w.id = j.value("id", w.id);
    w.name = j.value("name", w.name);
    w.comboWindow = j.value("combo_window", w.comboWindow);
    if (auto it = j.find("attacks"); it != j.end() && it->is_array()) {
        for (const auto& ja : *it) {
            if (ja.is_object()) w.attacks.push_back(attackFromJson(ja));
        }
    }
    return w;
}

WeaponDef WeaponDef::loadFromFile(const std::string& path) {
    std::string error;
    auto j = loadJsonFile(path, error);
    if (!j) {
        Log::error(LogCategory::Loading, "Waffe konnte nicht geladen werden: {}", error);
        return {};
    }
    WeaponDef w = fromJson(*j);
    if (!w.valid()) Log::error(LogCategory::Loading, "Waffe ohne Angriffe: {}", path);
    return w;
}

}  // namespace aldoria
