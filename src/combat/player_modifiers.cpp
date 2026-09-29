#include "combat/player_modifiers.h"

#include <algorithm>

#include "core/file_util.h"
#include "core/log.h"

namespace aldoria {

bool PlayerModifiers::apply(const std::string& effect, float v) {
    // Malwerte multiplizieren sich, Summenwerte addieren sich
    if (effect == "damage_mult") damageMult *= v;
    else if (effect == "attack_speed_mult") attackSpeedMult *= v;
    else if (effect == "move_speed_mult") moveSpeedMult *= v;
    else if (effect == "jump_mult") jumpMult *= v;
    else if (effect == "range_mult") rangeMult *= v;
    else if (effect == "knockback_mult") knockbackMult *= v;
    else if (effect == "dodge_cooldown_mult") dodgeCooldownMult *= v;
    else if (effect == "spark_damage_mult") sparkDamageMult *= v;
    else if (effect == "spark_cooldown_mult") sparkCooldownMult *= v;
    else if (effect == "dodge_iframes_add") dodgeIFramesAdd += v;
    else if (effect == "max_health_add") maxHealthAdd += v;
    else if (effect == "heal_on_kill") healOnKill += v;
    else if (effect == "flask_heal_add") flaskHealAdd += v;
    else if (effect == "resistance") resistance = std::min(0.9f, resistance + v);
    else if (effect == "crit_chance") critChance = std::min(1.0f, critChance + v);
    else return false;
    return true;
}

const SegenDef* SegenLibrary::find(const std::string& id) const {
    for (const SegenDef& s : all) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

PlayerModifiers SegenLibrary::combine(const std::vector<std::string>& chosen) const {
    PlayerModifiers m;
    for (const std::string& id : chosen) {
        const SegenDef* def = find(id);
        if (!def) {
            Log::warn(LogCategory::Player, "Unbekannter Segen '{}' im Spielstand, ignoriert", id);
            continue;
        }
        for (const auto& [effect, value] : def->effects) m.apply(effect, value);
    }
    return m;
}

SegenLibrary SegenLibrary::fromJson(const json& j) {
    SegenLibrary lib;
    auto it = j.find("segen");
    if (it == j.end() || !it->is_array()) return lib;
    for (const auto& js : *it) {
        if (!js.is_object() || !js.contains("id") || !js["id"].is_string()) continue;
        SegenDef s;
        s.id = js["id"].get<std::string>();
        s.name = js.value("name", s.id);
        s.description = js.value("description", std::string());
        if (auto eff = js.find("effects"); eff != js.end() && eff->is_object()) {
            for (auto e = eff->begin(); e != eff->end(); ++e) {
                if (!e.value().is_number()) continue;
                PlayerModifiers probe;
                if (!probe.apply(e.key(), e.value().get<float>())) {
                    Log::warn(LogCategory::Loading, "Segen '{}': unbekannter Effekt '{}'", s.id, e.key());
                    continue;
                }
                s.effects.emplace_back(e.key(), e.value().get<float>());
            }
        }
        lib.all.push_back(std::move(s));
    }
    return lib;
}

SegenLibrary SegenLibrary::loadFromFile(const std::string& path) {
    std::string error;
    auto j = loadJsonFile(path, error);
    if (!j) {
        Log::error(LogCategory::Loading, "Segen konnten nicht geladen werden: {}", error);
        return {};
    }
    return fromJson(*j);
}

}  // namespace aldoria
