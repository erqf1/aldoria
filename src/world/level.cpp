#include "world/level.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <random>

#include "core/file_util.h"
#include "core/json.h"
#include "core/json_util.h"
#include "raymath.h"

namespace aldoria {
namespace {

using namespace jsonutil;

// ---------------------------------------------------------------- Kollision

Vector2 riseDirection(RampDir d) {
    switch (d) {
        case RampDir::PlusX: return {1, 0};
        case RampDir::MinusX: return {-1, 0};
        case RampDir::PlusZ: return {0, 1};
        case RampDir::MinusZ: return {0, -1};
    }
    return {0, 1};
}

bool circleOverlapsRect(float minX, float maxX, float minZ, float maxZ, float x, float z, float radius) {
    float cx = std::clamp(x, minX, maxX);
    float cz = std::clamp(z, minZ, maxZ);
    float dx = x - cx, dz = z - cz;
    return dx * dx + dz * dz < radius * radius;
}

// Schiebt den Kreis (pos, radius) aus dem Rechteck. Gibt true zurück, wenn geschoben wurde.
bool pushOutOfRect(Vector3& pos, float minX, float maxX, float minZ, float maxZ, float radius) {
    float cx = std::clamp(pos.x, minX, maxX);
    float cz = std::clamp(pos.z, minZ, maxZ);
    float dx = pos.x - cx, dz = pos.z - cz;
    float d2 = dx * dx + dz * dz;
    if (d2 >= radius * radius) return false;

    if (d2 > 1e-8f) {
        float d = std::sqrt(d2);
        float push = radius - d;
        pos.x += dx / d * push;
        pos.z += dz / d * push;
    } else {
        // Mittelpunkt liegt im Rechteck: auf der kürzesten Seite hinausschieben
        float left = pos.x - minX, right = maxX - pos.x;
        float back = pos.z - minZ, front = maxZ - pos.z;
        float m = std::min({left, right, back, front});
        if (m == left) pos.x = minX - radius;
        else if (m == right) pos.x = maxX + radius;
        else if (m == back) pos.z = minZ - radius;
        else pos.z = maxZ + radius;
    }
    return true;
}

bool rayVsBox(Vector3 o, Vector3 d, const LevelBox& b, float maxDist, float& tHit) {
    const float origin[3] = {o.x, o.y, o.z};
    const float dir[3] = {d.x, d.y, d.z};
    const float lo[3] = {b.minX(), b.minY(), b.minZ()};
    const float hi[3] = {b.maxX(), b.maxY(), b.maxZ()};
    float tMin = 0.0f, tMax = maxDist;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(dir[i]) < 1e-8f) {
            if (origin[i] < lo[i] || origin[i] > hi[i]) return false;
        } else {
            float t1 = (lo[i] - origin[i]) / dir[i];
            float t2 = (hi[i] - origin[i]) / dir[i];
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }
    tHit = tMin;
    return true;
}

// Der Keil ist konvex: Strahl gegen fünf Halbräume (n·p <= d) beschneiden.
bool rayVsRamp(Vector3 o, Vector3 rayDir, const LevelRamp& r, float maxDist, float& tHit) {
    Vector2 dir = riseDirection(r.rise);
    bool alongX = r.rise == RampDir::PlusX || r.rise == RampDir::MinusX;
    float extent = alongX ? r.size.x : r.size.z;   // Länge entlang der Steigung
    float width = alongX ? r.size.z : r.size.x;    // Breite quer dazu
    float slope = r.size.y / extent;
    Vector2 side{-dir.y, dir.x};

    float tEnter = 0.0f, tExit = maxDist;
    auto clip = [&](Vector3 n, float d) {
        float denom = n.x * rayDir.x + n.y * rayDir.y + n.z * rayDir.z;
        float num = d - (n.x * o.x + n.y * o.y + n.z * o.z);
        if (std::fabs(denom) < 1e-8f) return num >= 0.0f;
        float t = num / denom;
        if (denom > 0.0f) tExit = std::min(tExit, t);
        else tEnter = std::max(tEnter, t);
        return tEnter <= tExit;
    };

    float cAlong = dir.x * r.center.x + dir.y * r.center.z;
    float cSide = side.x * r.center.x + side.y * r.center.z;
    bool hit = clip({0, -1, 0}, -r.baseY()) &&                                   // Unterseite
               clip({dir.x, 0, dir.y}, cAlong + extent * 0.5f) &&                // hohe Kante
               clip({side.x, 0, side.y}, cSide + width * 0.5f) &&                // Seiten
               clip({-side.x, 0, -side.y}, -cSide + width * 0.5f) &&
               clip({-slope * dir.x, 1, -slope * dir.y}, r.baseY() + r.size.y * 0.5f - slope * cAlong);  // Schräge
    if (!hit) return false;
    tHit = tEnter;
    return true;
}

// ---------------------------------------------------------------- Laden: Hilfen

bool readEnvironment(const json& j, Environment& env, std::string& err) {
    const std::string ctx = "environment";
    env.exposure = j.value("exposure", env.exposure);
    env.bloom = j.value("bloom", env.bloom);
    env.saturation = j.value("saturation", env.saturation);
    env.vignette = j.value("vignette", env.vignette);
    env.cloudAmount = j.value("cloud_amount", env.cloudAmount);
    env.sunGlow = j.value("sun_glow", env.sunGlow);
    env.ambientBoost = j.value("ambient_boost", env.ambientBoost);
    env.sunBoost = j.value("sun_boost", env.sunBoost);
    return readVec3(j, "sun_direction", env.sunDirection, ctx, err) &&
           readVec3(j, "sun_color", env.sunColor, ctx, err) &&
           readVec3(j, "sky_ambient", env.skyAmbient, ctx, err) &&
           readVec3(j, "ground_ambient", env.groundAmbient, ctx, err) &&
           readVec3(j, "fog_color", env.fogColor, ctx, err) &&
           readVec3(j, "sky_top_color", env.skyTopColor, ctx, err) &&
           readVec3(j, "grade", env.grade, ctx, err);
}

bool parseRampDir(const std::string& s, RampDir& out) {
    if (s == "+x") out = RampDir::PlusX;
    else if (s == "-x") out = RampDir::MinusX;
    else if (s == "+z") out = RampDir::PlusZ;
    else if (s == "-z") out = RampDir::MinusZ;
    else return false;
    return true;
}

bool parseDoorKind(const std::string& s, DoorKind& out) {
    if (s == "open") out = DoorKind::Open;
    else if (s == "key") out = DoorKind::Key;
    else if (s == "switch") out = DoorKind::Switch;
    else if (s == "arena") out = DoorKind::Arena;
    else if (s == "boss_defeated") out = DoorKind::BossDefeated;
    else if (s == "sealed") out = DoorKind::Sealed;
    else return false;
    return true;
}

Color defaultDoorColor(DoorKind k) {
    switch (k) {
        case DoorKind::Open: return {150, 128, 92, 255};
        case DoorKind::Key: return {170, 122, 62, 255};
        case DoorKind::Switch: return {84, 128, 196, 255};
        case DoorKind::Arena: return {176, 62, 60, 255};
        case DoorKind::BossDefeated: return {136, 62, 168, 255};
        case DoorKind::Sealed: return {96, 96, 104, 255};
    }
    return {150, 128, 92, 255};
}

struct Shell {
    bool present = false;
    float sx = 0, sz = 0, height = 7.0f, thickness = 1.0f, depth = 0.0f;
    Color color{104, 118, 92, 255};
};

// Türen auf einer Seite der Raumhülle als Lücken in der Wand
struct Gap {
    float lo, hi, top, bottom;
};

void addWall(Level& level, const Shell& shell, const std::string& side, std::vector<Gap> gaps) {
    const bool alongX = side == "north" || side == "south";
    // Nord/Süd reichen bis in die Ecken, Ost/West liegen dazwischen
    float lo = alongX ? -(shell.sx * 0.5f + shell.thickness) : -shell.sz * 0.5f;
    float hi = alongX ? (shell.sx * 0.5f + shell.thickness) : shell.sz * 0.5f;
    float fixedCoord = 0.0f;
    if (side == "north") fixedCoord = -(shell.sz * 0.5f + shell.thickness * 0.5f);
    else if (side == "south") fixedCoord = shell.sz * 0.5f + shell.thickness * 0.5f;
    else if (side == "east") fixedCoord = shell.sx * 0.5f + shell.thickness * 0.5f;
    else fixedCoord = -(shell.sx * 0.5f + shell.thickness * 0.5f);

    std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) { return a.lo < b.lo; });

    auto addSegment = [&](float a, float b, float y0, float y1, const char* tag) {
        if (b - a < 0.01f || y1 - y0 < 0.01f) return;
        LevelBox box;
        float mid = (a + b) * 0.5f, len = b - a, cy = (y0 + y1) * 0.5f, h = y1 - y0;
        if (alongX) {
            box.center = {mid, cy, fixedCoord};
            box.size = {len, h, shell.thickness};
        } else {
            box.center = {fixedCoord, cy, mid};
            box.size = {shell.thickness, h, len};
        }
        box.color = shell.color;
        box.tag = tag;
        level.boxes.push_back(std::move(box));
    };

    float cursor = lo;
    for (const Gap& g : gaps) {
        addSegment(cursor, g.lo, -shell.depth, shell.height, "wall");
        if (shell.depth > 0.0f) addSegment(g.lo, g.hi, -shell.depth, g.bottom, "wall");
        addSegment(g.lo, g.hi, g.top, shell.height, "lintel");
        cursor = std::max(cursor, g.hi);
    }
    addSegment(cursor, hi, -shell.depth, shell.height, "wall");
}

// ---------------------------------------------------------------- Laden: Abschnitte

bool parseShell(const json& j, Shell& shell, std::string& err) {
    auto it = j.find("shell");
    if (it == j.end()) return true;
    if (!it->is_object()) {
        err = "shell: erwartet ein Objekt";
        return false;
    }
    Vector2 size{0, 0};
    if (!readVec2(*it, "size", size, "shell", err) || !readColor(*it, "color", shell.color, "shell", err)) return false;
    if (size.x <= 0 || size.y <= 0) {
        err = "shell.size: [Breite, Tiefe] mit Werten größer 0 erwartet";
        return false;
    }
    shell.present = true;
    shell.sx = size.x;
    shell.sz = size.y;
    shell.height = it->value("height", shell.height);
    shell.thickness = it->value("thickness", shell.thickness);
    shell.depth = std::max(0.0f, it->value("depth", 0.0f));
    return true;
}

bool parseDoors(const json& j, const Shell& shell, Level& level, std::string& err) {
    auto it = j.find("doors");
    if (it == j.end()) {
        if (shell.present) {
            for (const char* side : {"north", "south", "east", "west"}) addWall(level, shell, side, {});
        }
        return true;
    }
    if (!it->is_array()) {
        err = "doors: erwartet eine Liste";
        return false;
    }
    std::vector<std::pair<std::string, Gap>> gapsBySide;
    size_t index = 0;
    for (const auto& jd : *it) {
        std::string ctx = std::format("doors[{}]", index++);
        if (!jd.is_object()) {
            err = ctx + ": erwartet ein Objekt";
            return false;
        }
        LevelDoor d;
        d.id = jd.value("id", std::string("door") + std::to_string(index));
        std::string kindName = jd.value("kind", std::string("open"));
        if (!parseDoorKind(kindName, d.kind)) {
            err = ctx + ".kind: unbekannt: " + kindName;
            return false;
        }
        d.color = defaultDoorColor(d.kind);
        if (!readColor(jd, "color", d.color, ctx, err)) return false;
        d.keyType = jd.value("key_type", d.keyType);
        d.switchId = jd.value("switch", std::string());
        if (std::string target = jd.value("target", std::string()); !target.empty()) {
            auto colon = target.find(':');
            d.targetRoom = target.substr(0, colon);
            if (colon != std::string::npos) d.targetDoor = target.substr(colon + 1);
        }

        float width = jd.value("width", 4.0f), height = jd.value("height", 4.5f), y = jd.value("y", 0.0f);
        std::string side = jd.value("side", std::string());
        if (!side.empty()) {
            if (!shell.present) {
                err = ctx + ": 'side' braucht eine 'shell' im Raum";
                return false;
            }
            float off = jd.value("x", 0.0f), t = shell.thickness;
            float cy = y + height * 0.5f;
            if (side == "north") {
                d.center = {off, cy, -(shell.sz * 0.5f + t * 0.5f)};
                d.size = {width, height, t};
                d.arrival = {off, y, -shell.sz * 0.5f + 2.5f};
                d.arrivalYaw = 0.0f;
            } else if (side == "south") {
                d.center = {off, cy, shell.sz * 0.5f + t * 0.5f};
                d.size = {width, height, t};
                d.arrival = {off, y, shell.sz * 0.5f - 2.5f};
                d.arrivalYaw = PI;
            } else if (side == "east") {
                d.center = {shell.sx * 0.5f + t * 0.5f, cy, off};
                d.size = {t, height, width};
                d.arrival = {shell.sx * 0.5f - 2.5f, y, off};
                d.arrivalYaw = -PI * 0.5f;
            } else if (side == "west") {
                d.center = {-(shell.sx * 0.5f + t * 0.5f), cy, off};
                d.size = {t, height, width};
                d.arrival = {-shell.sx * 0.5f + 2.5f, y, off};
                d.arrivalYaw = PI * 0.5f;
            } else {
                err = ctx + ".side: erlaubt sind north, south, east, west";
                return false;
            }
            gapsBySide.push_back({side, Gap{off - width * 0.5f, off + width * 0.5f, y + height, y}});
        } else {
            if (!jd.contains("center")) {
                err = ctx + ": entweder 'side' oder 'center' angeben";
                return false;
            }
            d.size = {width, height, jd.value("thickness", 1.0f)};
            if (!readVec3(jd, "center", d.center, ctx, err) || !readVec3(jd, "size", d.size, ctx, err) ||
                !readVec3(jd, "arrival", d.arrival, ctx, err)) {
                return false;
            }
            d.arrivalYaw = jd.value("arrival_yaw", 0.0f);
        }
        level.doors.push_back(std::move(d));
    }

    if (shell.present) {
        for (const char* side : {"north", "south", "east", "west"}) {
            std::vector<Gap> gaps;
            for (const auto& [s, g] : gapsBySide) {
                if (s == side) gaps.push_back(g);
            }
            addWall(level, shell, side, gaps);
        }
    }
    // Türboxen nach den Wänden anhängen
    for (LevelDoor& d : level.doors) {
        LevelBox box;
        box.center = d.center;
        box.size = d.size;
        box.color = d.color;
        box.tag = "door:" + d.id;
        d.boxIndex = (int)level.boxes.size();
        level.boxes.push_back(std::move(box));
    }
    return true;
}

bool parseSwitches(const json& j, Level& level, std::string& err) {
    auto it = j.find("switches");
    if (it == j.end()) return true;
    if (!it->is_array()) {
        err = "switches: erwartet eine Liste";
        return false;
    }
    size_t index = 0;
    for (const auto& js : *it) {
        std::string ctx = std::format("switches[{}]", index++);
        if (!js.is_object() || !js.contains("id") || !js.contains("position")) {
            err = ctx + ": 'id' und 'position' sind Pflicht";
            return false;
        }
        LevelSwitch s;
        s.id = js["id"].get<std::string>();
        std::string kind = js.value("kind", std::string("plate"));
        if (kind == "plate") s.kind = SwitchKind::Plate;
        else if (kind == "crystal") s.kind = SwitchKind::Crystal;
        else {
            err = ctx + ".kind: erlaubt sind plate und crystal";
            return false;
        }
        if (s.kind == SwitchKind::Crystal) s.size = {0.9f, 1.8f, 0.9f};
        if (!readVec3(js, "position", s.position, ctx, err) || !readVec3(js, "size", s.size, ctx, err)) return false;
        s.latch = js.value("latch", s.kind == SwitchKind::Crystal);
        s.holdTime = js.value("hold_time", 0.0f);

        LevelBox box;
        box.center = {s.position.x, s.position.y + s.size.y * 0.5f, s.position.z};
        box.size = s.size;
        box.solid = s.kind == SwitchKind::Crystal;
        box.visible = s.kind == SwitchKind::Plate;  // Kristalle zeichnet der Raum-Renderer selbst
        box.color = s.kind == SwitchKind::Plate ? Color{170, 150, 96, 255} : Color{92, 150, 190, 255};
        box.tag = "switch:" + s.id;
        s.boxIndex = (int)level.boxes.size();
        level.boxes.push_back(std::move(box));
        level.switches.push_back(std::move(s));
    }
    return true;
}

bool parsePlatforms(const json& j, Level& level, std::string& err) {
    auto it = j.find("platforms");
    if (it == j.end()) return true;
    if (!it->is_array()) {
        err = "platforms: erwartet eine Liste";
        return false;
    }
    size_t index = 0;
    for (const auto& jp : *it) {
        std::string ctx = std::format("platforms[{}]", index++);
        if (!jp.is_object()) {
            err = ctx + ": erwartet ein Objekt";
            return false;
        }
        LevelPlatform p;
        p.id = jp.value("id", std::string("platform") + std::to_string(index));
        std::string kind = jp.value("kind", std::string("moving"));
        if (kind == "moving") p.kind = PlatformKind::Moving;
        else if (kind == "crumble") p.kind = PlatformKind::Crumble;
        else {
            err = ctx + ".kind: erlaubt sind moving und crumble";
            return false;
        }
        p.color = p.kind == PlatformKind::Crumble ? Color{176, 140, 96, 255} : Color{150, 146, 132, 255};
        if (!readVec3(jp, "size", p.size, ctx, err) || !readColor(jp, "color", p.color, ctx, err)) return false;
        p.speed = jp.value("speed", p.speed);
        p.pause = jp.value("pause", p.pause);
        p.crumbleDelay = jp.value("crumble_delay", p.crumbleDelay);
        p.respawnTime = jp.value("respawn", p.respawnTime);

        if (auto pathIt = jp.find("path"); pathIt != jp.end()) {
            if (!pathIt->is_array() || pathIt->empty()) {
                err = ctx + ".path: erwartet eine Liste von [x, y, z]";
                return false;
            }
            for (const auto& wp : *pathIt) {
                if (!wp.is_array() || wp.size() != 3) {
                    err = ctx + ".path: jeder Wegpunkt braucht drei Zahlen";
                    return false;
                }
                p.path.push_back({wp[0].get<float>(), wp[1].get<float>(), wp[2].get<float>()});
            }
        } else {
            Vector3 c{0, 0, 0};
            if (!jp.contains("center") || !readVec3(jp, "center", c, ctx, err)) {
                if (err.empty()) err = ctx + ": 'center' oder 'path' ist Pflicht";
                return false;
            }
            p.path.push_back(c);
        }
        if (p.size.x <= 0 || p.size.y <= 0 || p.size.z <= 0) {
            err = ctx + ".size: alle Werte müssen größer als 0 sein";
            return false;
        }

        LevelBox box;
        box.center = p.path[0];
        box.size = p.size;
        box.color = p.color;
        box.tag = "platform:" + p.id;
        p.boxIndex = (int)level.boxes.size();
        level.boxes.push_back(std::move(box));
        level.platforms.push_back(std::move(p));
    }
    return true;
}

bool parseHazards(const json& j, Level& level, std::string& err) {
    auto it = j.find("hazards");
    if (it == j.end()) return true;
    if (!it->is_array()) {
        err = "hazards: erwartet eine Liste";
        return false;
    }
    size_t index = 0;
    for (const auto& jh : *it) {
        std::string ctx = std::format("hazards[{}]", index++);
        if (!jh.is_object() || !jh.contains("center") || !jh.contains("size")) {
            err = ctx + ": 'center' und 'size' sind Pflicht";
            return false;
        }
        LevelHazard h;
        std::string kind = jh.value("kind", std::string("spikes"));
        if (kind == "spikes") h.kind = HazardKind::Spikes;
        else if (kind == "lava") h.kind = HazardKind::Lava;
        else {
            err = ctx + ".kind: erlaubt sind spikes und lava";
            return false;
        }
        h.damage = jh.value("damage", h.kind == HazardKind::Lava ? 20.0f : 12.0f);
        if (!readVec3(jh, "center", h.center, ctx, err) || !readVec3(jh, "size", h.size, ctx, err)) return false;

        LevelBox box;
        box.center = h.center;
        box.size = h.size;
        box.solid = false;
        box.color = h.kind == HazardKind::Lava ? Color{238, 108, 40, 255} : Color{176, 178, 190, 255};
        box.tag = "hazard";
        h.boxIndex = (int)level.boxes.size();
        level.boxes.push_back(std::move(box));
        level.hazards.push_back(std::move(h));
    }
    return true;
}

bool parsePickups(const json& j, Level& level, std::string& err) {
    auto it = j.find("pickups");
    if (it == j.end()) return true;
    if (!it->is_array()) {
        err = "pickups: erwartet eine Liste";
        return false;
    }
    size_t index = 0;
    for (const auto& jp : *it) {
        std::string ctx = std::format("pickups[{}]", index++);
        if (!jp.is_object() || !jp.contains("type") || !jp.contains("position")) {
            err = ctx + ": 'type' und 'position' sind Pflicht";
            return false;
        }
        LevelPickup p;
        p.type = jp["type"].get<std::string>();
        p.id = jp.value("id", p.type + std::to_string(index));
        if (!readVec3(jp, "position", p.position, ctx, err)) return false;
        level.pickups.push_back(std::move(p));
    }
    return true;
}

bool parseEncounter(const json& j, Level& level, std::string& err) {
    auto it = j.find("encounter");
    if (it == j.end()) return true;
    if (!it->is_object() || !it->contains("waves") || !(*it)["waves"].is_array() || (*it)["waves"].empty()) {
        err = "encounter: 'waves' (Liste von Wellen) ist Pflicht";
        return false;
    }
    LevelEncounter e;
    size_t wi = 0;
    for (const auto& wave : (*it)["waves"]) {
        std::string ctx = std::format("encounter.waves[{}]", wi++);
        if (!wave.is_array() || wave.empty()) {
            err = ctx + ": erwartet eine Liste von {type, count}";
            return false;
        }
        std::vector<WaveEntry> entries;
        for (const auto& je : wave) {
            if (!je.is_object() || !je.contains("type") || !je["type"].is_string()) {
                err = ctx + ": jeder Eintrag braucht 'type'";
                return false;
            }
            entries.push_back({je["type"].get<std::string>(), std::max(1, je.value("count", 1))});
        }
        e.waves.push_back(std::move(entries));
    }
    if (auto sp = it->find("spawn_points"); sp != it->end()) {
        if (!sp->is_array()) {
            err = "encounter.spawn_points: erwartet eine Liste";
            return false;
        }
        for (const auto& p : *sp) {
            if (!p.is_array() || p.size() != 3) {
                err = "encounter.spawn_points: jeder Punkt braucht drei Zahlen";
                return false;
            }
            e.spawnPoints.push_back({p[0].get<float>(), p[1].get<float>(), p[2].get<float>()});
        }
    }
    if (e.spawnPoints.empty()) e.spawnPoints.push_back(level.playerSpawn);
    e.reward = it->value("reward", e.reward);
    level.encounter = std::move(e);
    return true;
}

bool parseDecor(const json& j, const Shell& shell, Level& level, std::string& err) {
    if (auto it = j.find("decor"); it != j.end()) {
        if (!it->is_array()) {
            err = "decor: erwartet eine Liste";
            return false;
        }
        size_t index = 0;
        for (const auto& jd : *it) {
            std::string ctx = std::format("decor[{}]", index++);
            if (!jd.is_object() || !jd.contains("kind") || !jd.contains("position")) {
                err = ctx + ": 'kind' und 'position' sind Pflicht";
                return false;
            }
            LevelDecor d;
            d.kind = jd["kind"].get<std::string>();
            d.scale = jd.value("scale", 1.0f);
            d.yaw = jd.value("yaw", 0.0f);
            if (!readVec3(jd, "position", d.position, ctx, err)) return false;
            level.decor.push_back(std::move(d));
        }
    }

    // Orte, die frei bleiben sollen: Gegner, Aufsammelbares, Türen, Schalter, Schrein, Boss
    auto nearSpecialPoint = [&level](Vector3 p) {
        auto near = [&p](Vector3 q) {
            float dx = p.x - q.x, dz = p.z - q.z;
            return dx * dx + dz * dz < 2.6f * 2.6f;
        };
        for (const EnemySpawn& s : level.enemySpawns) if (near(s.position)) return true;
        for (const LevelPickup& s : level.pickups) if (near(s.position)) return true;
        for (const LevelDoor& s : level.doors) if (near(s.arrival) || near(s.center)) return true;
        for (const LevelSwitch& s : level.switches) if (near(s.position)) return true;
        if (level.checkpoint && near(*level.checkpoint)) return true;
        if (level.boss && near(level.boss->position)) return true;
        if (level.encounter) for (const Vector3& s : level.encounter->spawnPoints) if (near(s)) return true;
        return false;
    };

    // Zufällig gestreute Dekoration mit fester Startzahl (jede Ladung sieht gleich aus)
    if (auto it = j.find("scatter"); it != j.end()) {
        if (!it->is_array()) {
            err = "scatter: erwartet eine Liste";
            return false;
        }
        float hx = (shell.present ? shell.sx : level.groundSize.x) * 0.5f;
        float hz = (shell.present ? shell.sz : level.groundSize.y) * 0.5f;
        float wall = shell.present ? shell.thickness : 0.0f;
        for (const auto& js : *it) {
            if (!js.is_object() || !js.contains("kind")) {
                err = "scatter: jeder Eintrag braucht 'kind'";
                return false;
            }
            std::string kind = js["kind"].get<std::string>();
            int count = std::clamp(js.value("count", 10), 0, 500);
            unsigned seed = js.value("seed", 1u);
            float margin = js.value("margin", 14.0f);
            std::string region = js.value("region", std::string("outside"));
            float minScale = js.value("min_scale", 0.8f), maxScale = js.value("max_scale", 1.5f);
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);

            int placed = 0;
            for (int attempt = 0; attempt < count * 30 && placed < count; attempt++) {
                Vector3 p{0, 0, 0};
                if (region == "outside") {
                    p.x = (unit(rng) * 2.0f - 1.0f) * (hx + wall + margin);
                    p.z = (unit(rng) * 2.0f - 1.0f) * (hz + wall + margin);
                    if (std::fabs(p.x) < hx + wall + 1.5f && std::fabs(p.z) < hz + wall + 1.5f) continue;
                } else {
                    float minR = js.value("min_radius", 0.0f), maxR = js.value("max_radius", 0.0f);
                    if (maxR > 0.0f) {
                        // Ring um die Mitte (z. B. Kulisse hinter dem Spielbereich)
                        float r = minR + unit(rng) * (maxR - minR), a = unit(rng) * 6.2831853f;
                        p.x = std::cos(a) * r;
                        p.z = std::sin(a) * r;
                    } else {
                        p.x = (unit(rng) * 2.0f - 1.0f) * (hx - 1.5f);
                        p.z = (unit(rng) * 2.0f - 1.0f) * (hz - 1.5f);
                    }
                    float dxs = p.x - level.playerSpawn.x, dzs = p.z - level.playerSpawn.z;
                    if (dxs * dxs + dzs * dzs < 16.0f || nearSpecialPoint(p)) continue;
                    bool blocked = false;
                    for (const LevelBox& b : level.boxes) {
                        if (p.x > b.minX() - 1.0f && p.x < b.maxX() + 1.0f && p.z > b.minZ() - 1.0f && p.z < b.maxZ() + 1.0f) {
                            blocked = true;
                            break;
                        }
                    }
                    for (const LevelRamp& rp : level.ramps) {
                        if (p.x > rp.minX() - 1.2f && p.x < rp.maxX() + 1.2f && p.z > rp.minZ() - 1.2f && p.z < rp.maxZ() + 1.2f) blocked = true;
                    }
                    // Fährwege beweglicher Plattformen bleiben frei
                    for (const LevelPlatform& pl : level.platforms) {
                        for (const Vector3& c : pl.path) {
                            if (std::fabs(p.x - c.x) < pl.size.x * 0.5f + 1.5f && std::fabs(p.z - c.z) < pl.size.z * 0.5f + 1.5f) blocked = true;
                        }
                        if (pl.path.size() >= 2) {
                            // Strecke zwischen erstem und letztem Punkt: grob als Rechteck prüfen
                            const Vector3& a = pl.path.front();
                            const Vector3& b = pl.path.back();
                            float minX = std::min(a.x, b.x) - pl.size.x * 0.5f - 1.5f, maxX = std::max(a.x, b.x) + pl.size.x * 0.5f + 1.5f;
                            float minZ = std::min(a.z, b.z) - pl.size.z * 0.5f - 1.5f, maxZ = std::max(a.z, b.z) + pl.size.z * 0.5f + 1.5f;
                            if (p.x > minX && p.x < maxX && p.z > minZ && p.z < maxZ) blocked = true;
                        }
                    }
                    if (blocked) continue;
                }
                LevelDecor d;
                d.kind = kind;
                d.position = p;
                d.scale = minScale + unit(rng) * (maxScale - minScale);
                d.yaw = unit(rng) * 6.2831853f;
                level.decor.push_back(std::move(d));
                placed++;
            }
        }
    }

    // Felsen im Spielbereich sind Hindernisse (unsichtbare Box unter der Form)
    for (const LevelDecor& d : level.decor) {
        if (d.kind != "rock") continue;
        if (std::fabs(d.position.x) > level.groundSize.x * 0.5f - 1.0f || std::fabs(d.position.z) > level.groundSize.y * 0.5f - 1.0f) continue;
        LevelBox box;
        box.center = {d.position.x, level.groundY + 0.45f * d.scale, d.position.z};
        box.size = {1.7f * d.scale, 0.9f * d.scale, 1.5f * d.scale};
        box.visible = false;
        box.tag = "decor_rock";
        level.boxes.push_back(std::move(box));
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- Rampe

float LevelRamp::surfaceHeight(float x, float z) const {
    Vector2 dir = riseDirection(rise);
    bool alongX = rise == RampDir::PlusX || rise == RampDir::MinusX;
    float extent = alongX ? size.x : size.z;
    float cx = std::clamp(x, minX(), maxX());
    float cz = std::clamp(z, minZ(), maxZ());
    float along = (cx - center.x) * dir.x + (cz - center.z) * dir.y;
    float t = std::clamp(along / extent + 0.5f, 0.0f, 1.0f);
    return baseY() + t * size.y;
}

// ---------------------------------------------------------------- Level

void Level::resolveHorizontal(Vector3& pos, float radius, float height) const {
    for (int iteration = 0; iteration < 3; iteration++) {
        for (const LevelBox& b : boxes) {
            if (!b.solid) continue;
            // Stufen und Boxen ganz über dem Kopf sind keine Hindernisse
            if (b.maxY() <= pos.y + kStepHeight || b.minY() >= pos.y + height) continue;
            pushOutOfRect(pos, b.minX(), b.maxX(), b.minZ(), b.maxZ(), radius);
        }
        for (const LevelRamp& r : ramps) {
            if (r.baseY() >= pos.y + height) continue;
            if (!circleOverlapsRect(r.minX(), r.maxX(), r.minZ(), r.maxZ(), pos.x, pos.z, radius)) continue;
            // Wie bei Boxen: Ist die Oberfläche am nächsten Punkt nur eine Stufe hoch, ist sie begehbar
            if (r.surfaceHeight(pos.x, pos.z) <= pos.y + kStepHeight) continue;
            pushOutOfRect(pos, r.minX(), r.maxX(), r.minZ(), r.maxZ(), radius);
        }
    }
    float halfX = groundSize.x * 0.5f - radius, halfZ = groundSize.y * 0.5f - radius;
    pos.x = std::clamp(pos.x, -halfX, halfX);
    pos.z = std::clamp(pos.z, -halfZ, halfZ);
}

float Level::groundHeight(Vector3 pos, float radius, float feetY) const {
    float best = groundY;
    for (const LevelBox& b : boxes) {
        if (!b.solid) continue;
        if (b.maxY() > feetY + kStepHeight) continue;  // zu hoch: Wand, keine Standfläche
        if (!circleOverlapsRect(b.minX(), b.maxX(), b.minZ(), b.maxZ(), pos.x, pos.z, radius)) continue;
        best = std::max(best, b.maxY());
    }
    for (const LevelRamp& r : ramps) {
        if (!circleOverlapsRect(r.minX(), r.maxX(), r.minZ(), r.maxZ(), pos.x, pos.z, radius)) continue;
        float h = r.surfaceHeight(pos.x, pos.z);
        if (h > feetY + kStepHeight) continue;
        best = std::max(best, h);
    }
    return best;
}

bool Level::raycast(Vector3 origin, Vector3 dir, float maxDist, float& hitDist) const {
    bool hit = false;
    float best = maxDist;
    for (const LevelBox& b : boxes) {
        if (!b.solid) continue;
        float t;
        if (rayVsBox(origin, dir, b, best, t) && t < best) {
            best = t;
            hit = true;
        }
    }
    for (const LevelRamp& r : ramps) {
        float t;
        if (rayVsRamp(origin, dir, r, best, t) && t < best) {
            best = t;
            hit = true;
        }
    }
    if (hit) hitDist = best;
    return hit;
}

bool Level::standsOn(const LevelBox& box, Vector3 feet, float radius) {
    if (!box.solid) return false;
    if (std::fabs(feet.y - box.maxY()) > 0.06f) return false;
    return circleOverlapsRect(box.minX(), box.maxX(), box.minZ(), box.maxZ(), feet.x, feet.z, radius);
}

const LevelDoor* Level::findDoor(const std::string& doorId) const {
    for (const LevelDoor& d : doors) {
        if (d.id == doorId) return &d;
    }
    return nullptr;
}

Level Level::makeFallback() {
    Level l;
    l.playerSpawn = {0, 0, 0};
    return l;
}

// ---------------------------------------------------------------- Laden

// Punktlichter aus Dekoration, Schrein und Lava ableiten
static void buildLights(Level& level) {
    for (const LevelDecor& d : level.decor) {
        LevelLight l;
        l.position = d.position;
        if (d.kind == "torch") {
            l.position.y += 1.7f * d.scale;
            l.color = {2.6f, 1.25f, 0.45f};
            l.radius = 10.0f;
            l.flicker = 1.0f;
        } else if (d.kind == "brazier") {
            l.position.y += 1.5f * d.scale;
            l.color = {3.0f, 1.3f, 0.4f};
            l.radius = 12.0f;
            l.flicker = 1.0f;
        } else if (d.kind == "lamp") {
            l.position.y += 1.8f * d.scale;
            l.color = {0.9f, 1.8f, 2.6f};
            l.radius = 9.0f;
            l.flicker = 0.2f;
        } else if (d.kind == "crystal_cluster") {
            l.position.y += 0.8f * d.scale;
            l.color = {0.5f, 1.2f, 1.8f};
            l.radius = 6.0f;
        } else {
            continue;
        }
        level.lights.push_back(l);
    }
    if (level.checkpoint) {
        LevelLight l;
        l.position = {level.checkpoint->x, level.checkpoint->y + 1.7f, level.checkpoint->z};
        l.color = {0.9f, 1.6f, 2.2f};
        l.radius = 10.0f;
        l.flicker = 0.15f;
        level.lights.push_back(l);
    }
    for (const LevelHazard& h : level.hazards) {
        if (h.kind != HazardKind::Lava) continue;
        // Über die Lava verteilt einige Lichter (nur die nächsten werden beim Zeichnen genutzt)
        int nx = std::clamp((int)std::ceil(h.size.x / 7.0f), 1, 6), nz = std::clamp((int)std::ceil(h.size.z / 7.0f), 1, 6);
        for (int ix = 0; ix < nx; ix++) {
            for (int iz = 0; iz < nz; iz++) {
                LevelLight l;
                l.position = {h.center.x - h.size.x * 0.5f + (ix + 0.5f) * h.size.x / (float)nx, h.center.y + h.size.y * 0.5f + 0.8f,
                              h.center.z - h.size.z * 0.5f + (iz + 0.5f) * h.size.z / (float)nz};
                l.color = {2.4f, 0.75f, 0.18f};
                l.radius = 9.0f;
                l.flicker = 0.5f;
                level.lights.push_back(l);
            }
        }
    }
}

LevelLoadResult loadLevelFromJson(const std::string& text) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return {std::nullopt, "Level-JSON ist ungültig"};

    Level level;
    std::string err;
    level.id = j.value("id", std::string("unnamed"));
    level.name = j.value("name", level.id);
    level.type = j.value("type", level.type);
    if (level.id.rfind("gs", 0) == 0) level.theme = "schmiede";
    else if (level.id.rfind("ht", 0) == 0) level.theme = "himmel";
    level.theme = j.value("theme", level.theme);
    // Grundstimmung je Thema; die Werte im Abschnitt "environment" des Raums haben Vorrang
    if (level.theme == "schmiede") {
        level.env.exposure = 0.95f;
        level.env.sunBoost = 0.55f;
        level.env.ambientBoost = 0.85f;
        level.env.bloom = 0.7f;
        level.env.cloudAmount = 0.6f;
    } else if (level.theme == "himmel") {
        level.env.exposure = 0.55f;
        level.env.sunBoost = 0.5f;
        level.env.ambientBoost = 0.5f;
        level.env.bloom = 0.4f;
        level.env.cloudAmount = 0.55f;
    } else {
        level.env.exposure = 0.85f;
        level.env.sunBoost = 0.72f;
        level.env.ambientBoost = 0.8f;
        level.env.saturation = 0.95f;
    }

    if (auto it = j.find("environment"); it != j.end()) {
        if (!it->is_object() || !readEnvironment(*it, level.env, err)) {
            return {std::nullopt, err.empty() ? "environment: erwartet ein Objekt" : err};
        }
        level.env.fogStart = it->value("fog_start", level.env.fogStart);
        level.env.fogEnd = it->value("fog_end", level.env.fogEnd);
    }
    if (!readVec3(j, "player_spawn", level.playerSpawn, "level", err)) return {std::nullopt, err};
    level.playerSpawnYaw = j.value("player_yaw", level.playerSpawnYaw);

    Shell shell;
    if (!parseShell(j, shell, err)) return {std::nullopt, err};
    if (shell.present) {
        level.shellSize = {shell.sx, shell.sz};
        if (auto sh = j.find("shell"); sh != j.end() && sh->contains("cap_color")) {
            if (!readColor(*sh, "cap_color", level.wallCap, "shell", err)) return {std::nullopt, err};
        }
        level.groundSize = {shell.sx + 2.0f * shell.thickness, shell.sz + 2.0f * shell.thickness};
    }

    if (auto it = j.find("ground"); it != j.end()) {
        float size[2] = {level.groundSize.x, level.groundSize.y};
        if (!it->is_object() || !readNumbers(*it, "size", 2, size, "ground", err) ||
            !readColor(*it, "color", level.groundColor, "ground", err)) {
            return {std::nullopt, err.empty() ? "ground: erwartet ein Objekt" : err};
        }
        level.groundSize = {size[0], size[1]};
        level.groundY = it->value("y", 0.0f);
        level.hasGround = it->value("enabled", true);
    }
    if (!level.hasGround) level.groundY = -10000.0f;  // nichts trägt: man fällt bis zur Absturzhöhe
    level.killY = j.value("kill_y", level.hasGround ? level.groundY - 30.0f : -14.0f);

    if (auto it = j.find("boxes"); it != j.end()) {
        if (!it->is_array()) return {std::nullopt, "boxes: erwartet eine Liste"};
        size_t index = 0;
        for (const auto& jb : *it) {
            std::string ctx = std::format("boxes[{}]", index++);
            if (!jb.is_object()) return {std::nullopt, ctx + ": erwartet ein Objekt"};
            if (!jb.contains("center") || !jb.contains("size")) {
                return {std::nullopt, ctx + ": 'center' und 'size' sind Pflicht"};
            }
            LevelBox box;
            if (!readVec3(jb, "center", box.center, ctx, err) || !readVec3(jb, "size", box.size, ctx, err) ||
                !readColor(jb, "color", box.color, ctx, err)) {
                return {std::nullopt, err};
            }
            if (box.size.x <= 0 || box.size.y <= 0 || box.size.z <= 0) {
                return {std::nullopt, ctx + ".size: alle Werte müssen größer als 0 sein"};
            }
            box.solid = jb.value("solid", true);
            box.tag = jb.value("tag", std::string());
            level.boxes.push_back(std::move(box));
        }
    }

    if (auto it = j.find("ramps"); it != j.end()) {
        if (!it->is_array()) return {std::nullopt, "ramps: erwartet eine Liste"};
        size_t index = 0;
        for (const auto& jr : *it) {
            std::string ctx = std::format("ramps[{}]", index++);
            if (!jr.is_object() || !jr.contains("center") || !jr.contains("size") || !jr.contains("rise")) {
                return {std::nullopt, ctx + ": 'center', 'size' und 'rise' sind Pflicht"};
            }
            LevelRamp ramp;
            if (!readVec3(jr, "center", ramp.center, ctx, err) || !readVec3(jr, "size", ramp.size, ctx, err) ||
                !readColor(jr, "color", ramp.color, ctx, err)) {
                return {std::nullopt, err};
            }
            if (ramp.size.x <= 0 || ramp.size.y <= 0 || ramp.size.z <= 0) {
                return {std::nullopt, ctx + ".size: alle Werte müssen größer als 0 sein"};
            }
            if (!jr["rise"].is_string() || !parseRampDir(jr["rise"].get<std::string>(), ramp.rise)) {
                return {std::nullopt, ctx + ".rise: erlaubt sind \"+x\", \"-x\", \"+z\", \"-z\""};
            }
            level.ramps.push_back(ramp);
        }
    }

    if (auto it = j.find("enemies"); it != j.end()) {
        if (!it->is_array()) return {std::nullopt, "enemies: erwartet eine Liste"};
        size_t index = 0;
        for (const auto& je : *it) {
            std::string ctx = std::format("enemies[{}]", index++);
            if (!je.is_object() || !je.contains("type") || !je["type"].is_string()) {
                return {std::nullopt, ctx + ": 'type' ist Pflicht"};
            }
            EnemySpawn spawn;
            spawn.type = je["type"].get<std::string>();
            if (!readVec3(je, "position", spawn.position, ctx, err)) return {std::nullopt, err};
            level.enemySpawns.push_back(std::move(spawn));
        }
    }

    if (!parseDoors(j, shell, level, err) || !parseSwitches(j, level, err) || !parsePlatforms(j, level, err) ||
        !parseHazards(j, level, err) || !parsePickups(j, level, err) || !parseEncounter(j, level, err)) {
        return {std::nullopt, err};
    }

    if (auto it = j.find("arena_spawn_points"); it != j.end()) {
        if (!it->is_array()) return {std::nullopt, "arena_spawn_points: erwartet eine Liste"};
        for (const auto& p : *it) {
            if (!p.is_array() || p.size() != 3) return {std::nullopt, "arena_spawn_points: jeder Punkt braucht drei Zahlen"};
            level.arenaSpawnPoints.push_back({p[0].get<float>(), p[1].get<float>(), p[2].get<float>()});
        }
    }
    if (auto it = j.find("checkpoint"); it != j.end()) {
        Vector3 p{0, 0, 0};
        if (!it->is_object() || !it->contains("position") || !readVec3(*it, "position", p, "checkpoint", err)) {
            return {std::nullopt, err.empty() ? "checkpoint: 'position' ist Pflicht" : err};
        }
        level.checkpoint = p;
    }
    if (auto it = j.find("boss"); it != j.end()) {
        if (!it->is_object() || !it->contains("type") || !(*it)["type"].is_string()) {
            return {std::nullopt, "boss: 'type' ist Pflicht"};
        }
        LevelBossSpawn b;
        b.type = (*it)["type"].get<std::string>();
        if (!readVec3(*it, "position", b.position, "boss", err)) return {std::nullopt, err};
        level.boss = std::move(b);
    }
    if (!parseDecor(j, shell, level, err)) return {std::nullopt, err};
    buildLights(level);

    return {std::move(level), ""};
}

LevelLoadResult loadLevelFromFile(const std::string& path) {
    auto text = readTextFile(path);
    if (!text) return {std::nullopt, "Level-Datei nicht gefunden: " + path};
    return loadLevelFromJson(*text);
}

}  // namespace aldoria
