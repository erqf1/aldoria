#include "world/room.h"

#include <algorithm>
#include <cmath>

#include "core/events.h"
#include "core/log.h"
#include "raymath.h"

namespace aldoria {
namespace {

constexpr float kPickupRadius = 1.1f;
constexpr float kEncounterEnterDelay = 0.8f;
constexpr float kCountdownTime = 1.6f;
constexpr float kBetweenWavesTime = 1.6f;

bool circleOverlapsRect(float minX, float maxX, float minZ, float maxZ, float x, float z, float radius) {
    float cx = std::clamp(x, minX, maxX);
    float cz = std::clamp(z, minZ, maxZ);
    float dx = x - cx, dz = z - cz;
    return dx * dx + dz * dz < radius * radius;
}

float distanceToRect(float minX, float maxX, float minZ, float maxZ, float x, float z) {
    float dx = x - std::clamp(x, minX, maxX);
    float dz = z - std::clamp(z, minZ, maxZ);
    return std::sqrt(dx * dx + dz * dz);
}

Color withAlpha(Color c, unsigned char a) {
    c.a = a;
    return c;
}

Color lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), a.a};
}

}  // namespace

Room::Room(Level level, DungeonState& state, std::string roomId)
    : level_(std::move(level)), state_(state), roomId_(std::move(roomId)) {
    platforms_.resize(level_.platforms.size());
    for (size_t i = 0; i < platforms_.size(); i++) {
        const LevelPlatform& def = level_.platforms[i];
        platforms_[i].home = def.path.empty() ? level_.boxes[def.boxIndex].center : def.path[0];
        platforms_[i].next = def.path.size() > 1 ? 1 : 0;
    }

    switches_.resize(level_.switches.size());
    for (size_t i = 0; i < switches_.size(); i++) {
        if (state_.latchedSwitches.count(DungeonState::key(roomId_, level_.switches[i].id))) {
            switches_[i].latched = switches_[i].active = true;
        }
    }

    doors_.resize(level_.doors.size());

    for (const LevelPickup& lp : level_.pickups) {
        RoomPickup p;
        p.def = lp;
        p.collected = state_.collected.count(DungeonState::key(roomId_, lp.id)) > 0;
        pickups_.push_back(std::move(p));
    }

    if (level_.encounter) {
        encounterPhase_ = state_.isCleared(roomId_) ? EncounterPhase::Cleared : EncounterPhase::Waiting;
    }
    bossDefeated_ = level_.boss && state_.defeatedBosses.count(level_.boss->type) > 0;
}

bool Room::inCombat() const {
    return bossFight_ || encounterPhase_ == EncounterPhase::Countdown || encounterPhase_ == EncounterPhase::Fighting ||
           encounterPhase_ == EncounterPhase::Between;
}

bool Room::switchActive(const std::string& id) const {
    for (size_t i = 0; i < level_.switches.size(); i++) {
        if (level_.switches[i].id == id) return switches_[i].active;
    }
    return false;
}

void Room::update(float dt, std::vector<RoomActor>& actors, int aliveEnemies, EventBus& events) {
    if (dt <= 0.0f) return;
    time_ += dt;

    const RoomActor* player = nullptr;
    for (const RoomActor& a : actors) {
        if (a.isPlayer && a.alive && a.body) player = &a;
    }

    updatePlatforms(dt, actors, events);
    updateSwitches(dt, actors, events);
    updateEncounter(dt, player, aliveEnemies, events);
    updateDoors(player, events);
    updatePickups(player, events);
    firstUpdate_ = false;
}

// ---------------------------------------------------------------- Plattformen

void Room::updatePlatforms(float dt, std::vector<RoomActor>& actors, EventBus& events) {
    for (size_t i = 0; i < level_.platforms.size(); i++) {
        const LevelPlatform& def = level_.platforms[i];
        LevelBox& box = level_.boxes[def.boxIndex];
        PlatformRuntime& rt = platforms_[i];

        if (def.kind == PlatformKind::Moving) {
            if (def.path.size() < 2) continue;
            if (rt.pauseTimer > 0.0f) {
                rt.pauseTimer -= dt;
                continue;
            }
            Vector3 to = Vector3Subtract(def.path[rt.next], box.center);
            float dist = Vector3Length(to);
            float step = def.speed * dt;
            Vector3 delta;
            if (dist <= step) {
                delta = to;
                rt.pauseTimer = def.pause;
                int n = (int)def.path.size();
                if (rt.direction > 0) {
                    if ((int)rt.next >= n - 1) {
                        rt.direction = -1;
                        rt.next = (size_t)(n - 2);
                    } else {
                        rt.next++;
                    }
                } else {
                    if (rt.next == 0) {
                        rt.direction = 1;
                        rt.next = 1;
                    } else {
                        rt.next--;
                    }
                }
            } else {
                delta = Vector3Scale(to, step / dist);
            }
            // Wer darauf steht, wird mitgenommen (vor dem Bewegen prüfen)
            for (RoomActor& a : actors) {
                if (a.body && Level::standsOn(box, a.body->position, a.body->radius)) {
                    a.body->position = Vector3Add(a.body->position, delta);
                }
            }
            box.center = Vector3Add(box.center, delta);
            continue;
        }

        // Bröckelnde Plattform
        switch (rt.crumble) {
            case PlatformRuntime::Crumble::Solid:
                for (const RoomActor& a : actors) {
                    if (a.alive && a.body && Level::standsOn(box, a.body->position, a.body->radius)) {
                        rt.crumble = PlatformRuntime::Crumble::Warning;
                        rt.timer = def.crumbleDelay;
                        break;
                    }
                }
                break;
            case PlatformRuntime::Crumble::Warning: {
                rt.timer -= dt;
                float flash = 0.5f + 0.5f * std::sin(time_ * 40.0f);
                box.color = lerpColor(def.color, Color{214, 84, 60, 255}, 0.35f + 0.5f * flash);
                if (rt.timer <= 0.0f) {
                    rt.crumble = PlatformRuntime::Crumble::Falling;
                    rt.timer = 0.7f;
                    box.solid = false;
                    events.emit(PlatformCrumbled{def.id});
                }
                break;
            }
            case PlatformRuntime::Crumble::Falling:
                rt.timer -= dt;
                box.center.y -= 6.0f * dt;
                if (rt.timer <= 0.0f) {
                    rt.crumble = PlatformRuntime::Crumble::Gone;
                    rt.timer = def.respawnTime;
                    box.visible = false;
                }
                break;
            case PlatformRuntime::Crumble::Gone:
                rt.timer -= dt;
                if (rt.timer <= 0.0f) {
                    box.center = rt.home;
                    box.visible = true;
                    box.solid = true;
                    box.color = def.color;
                    rt.crumble = PlatformRuntime::Crumble::Solid;
                }
                break;
        }
    }
}

// ---------------------------------------------------------------- Schalter

void Room::latchSwitch(size_t index, EventBus& events) {
    SwitchRuntime& rt = switches_[index];
    if (rt.latched) return;
    rt.latched = true;
    state_.latchedSwitches.insert(DungeonState::key(roomId_, level_.switches[index].id));
    if (!rt.active) {
        rt.active = true;
        events.emit(SwitchToggled{level_.switches[index].id, true});
    }
}

void Room::updateSwitches(float dt, const std::vector<RoomActor>& actors, EventBus& events) {
    for (size_t i = 0; i < level_.switches.size(); i++) {
        const LevelSwitch& def = level_.switches[i];
        SwitchRuntime& rt = switches_[i];
        LevelBox& box = level_.boxes[def.boxIndex];
        bool wasActive = rt.active;

        if (def.kind == SwitchKind::Plate) {
            bool pressed = false;
            for (const RoomActor& a : actors) {
                if (!a.alive || !a.body) continue;
                Vector3 p = a.body->position;
                bool heightOk = p.y >= def.position.y - 0.15f && p.y <= def.position.y + def.size.y + 0.4f;
                if (heightOk && circleOverlapsRect(box.minX(), box.maxX(), box.minZ(), box.maxZ(), p.x, p.z,
                                                   a.body->radius * 0.6f)) {
                    pressed = true;
                    break;
                }
            }
            if (pressed) {
                rt.holdTimer = def.holdTime;
                rt.active = true;
                if (def.latch) latchSwitch(i, events);
            } else if (rt.holdTimer > 0.0f) {
                rt.holdTimer -= dt;
                rt.active = rt.holdTimer > 0.0f || rt.latched;
            } else {
                rt.active = rt.latched;
            }
            box.color = rt.active ? Color{110, 214, 122, 255} : Color{170, 150, 96, 255};
        } else {
            rt.active = rt.latched;
            box.color = rt.active ? Color{130, 240, 255, 255} : Color{92, 150, 190, 255};
        }
        if (rt.active != wasActive && !firstUpdate_) events.emit(SwitchToggled{def.id, rt.active});
    }
}

void Room::hitCrystals(const MeleeQuery& query, EventBus& events) {
    for (size_t i = 0; i < level_.switches.size(); i++) {
        const LevelSwitch& def = level_.switches[i];
        if (def.kind != SwitchKind::Crystal || switches_[i].latched) continue;
        Vector3 base{def.position.x, def.position.y, def.position.z};
        if (meleeHits(query, base, 0.5f, def.size.y)) latchSwitch(i, events);
    }
}

void Room::hitCrystalsAt(Vector3 position, float radius, EventBus& events) {
    for (size_t i = 0; i < level_.switches.size(); i++) {
        const LevelSwitch& def = level_.switches[i];
        if (def.kind != SwitchKind::Crystal || switches_[i].latched) continue;
        Vector3 center{def.position.x, def.position.y + def.size.y * 0.5f, def.position.z};
        if (Vector3Distance(center, position) < radius + 0.7f) latchSwitch(i, events);
    }
}

// ---------------------------------------------------------------- Türen

void Room::applyDoorState(size_t index, bool open, bool notify, EventBus& events) {
    const LevelDoor& d = level_.doors[index];
    LevelBox& box = level_.boxes[d.boxIndex];
    bool changed = doors_[index].open != open;
    doors_[index].open = open;
    box.solid = !open;
    if (open) {
        // Offene Türen mit Ziel leuchten durchsichtig als Durchgang, ohne Ziel verschwinden sie
        box.visible = !d.targetRoom.empty();
        box.color = withAlpha(d.color, 70);
    } else {
        box.visible = true;
        box.color = withAlpha(d.color, 255);
    }
    if (changed && open && notify) events.emit(DoorOpened{roomId_, d.id});
}

bool Room::desiredDoorOpen(size_t index, const RoomActor* player, EventBus& events) {
    const LevelDoor& d = level_.doors[index];
    switch (d.kind) {
        case DoorKind::Open: return true;
        case DoorKind::Sealed: return false;
        case DoorKind::Switch: {
            // "a+b": alle genannten Schalter müssen aktiv sein
            size_t start = 0;
            const std::string& ids = d.switchId;
            while (start <= ids.size()) {
                size_t plus = ids.find('+', start);
                std::string one = ids.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
                if (!one.empty() && !switchActive(one)) return false;
                if (plus == std::string::npos) break;
                start = plus + 1;
            }
            return !ids.empty();
        }
        case DoorKind::Arena: return !inCombat();
        case DoorKind::BossDefeated: return bossDefeated_;
        case DoorKind::Key: {
            std::string key = DungeonState::key(roomId_, d.id);
            if (state_.openedDoors.count(key)) return true;
            if (!player) return false;
            const LevelBox& box = level_.boxes[d.boxIndex];
            Vector3 p = player->body->position;
            float dist = distanceToRect(box.minX(), box.maxX(), box.minZ(), box.maxZ(), p.x, p.z);
            if (dist > 1.6f) return false;
            bool bossKey = d.keyType == "boss_key";
            bool hasKey = bossKey ? state_.bossKey : state_.smallKeys > 0;
            if (!hasKey) return false;
            if (bossKey) state_.bossKey = false;
            else state_.smallKeys--;
            state_.openedDoors.insert(key);
            events.emit(DoorUnlocked{roomId_, d.id});
            return true;
        }
    }
    return false;
}

void Room::updateDoors(const RoomActor* player, EventBus& events) {
    for (size_t i = 0; i < level_.doors.size(); i++) {
        bool open = desiredDoorOpen(i, player, events);
        if (open != doors_[i].open || firstUpdate_) applyDoorState(i, open, !firstUpdate_, events);
    }

    if (transition_ || !player || time_ < 0.6f) return;
    Vector3 p = player->body->position;
    for (size_t i = 0; i < level_.doors.size(); i++) {
        const LevelDoor& d = level_.doors[i];
        if (!doors_[i].open || d.targetRoom.empty()) continue;
        const LevelBox& box = level_.boxes[d.boxIndex];
        constexpr float kInflate = 0.7f;
        bool inside = p.x > box.minX() - kInflate && p.x < box.maxX() + kInflate && p.z > box.minZ() - kInflate &&
                      p.z < box.maxZ() + kInflate && p.y > box.minY() - 0.5f && p.y < box.maxY();
        if (inside) {
            transition_ = RoomTransition{d.targetRoom, d.targetDoor};
            return;
        }
    }
}

std::optional<RoomTransition> Room::takeTransition() {
    auto t = transition_;
    transition_.reset();
    return t;
}

// ---------------------------------------------------------------- Aufsammelbares

void Room::applyPickupEffect(const RoomPickup& p, EventBus& events) {
    const std::string& t = p.def.type;
    if (t == "small_key") {
        state_.smallKeys++;
    } else if (t == "boss_key") {
        state_.bossKey = true;
    } else if (t == "shard") {
        state_.shards++;
    } else if (t == "heart") {
        state_.heartContainers++;
    } else if (t == "flask") {
        state_.flaskMax++;
        state_.flaskCharges++;
    } else if (t == "potion") {
        state_.flaskCharges = std::min(state_.flaskMax, state_.flaskCharges + 1);
    } else if (t == "item_dash") {
        if (state_.abilities.insert("dash").second) events.emit(AbilityUnlocked{"dash"});
    } else if (t == "item_double_jump") {
        if (state_.abilities.insert("double_jump").second) events.emit(AbilityUnlocked{"double_jump"});
    } else if (t == "health_drop") {
        // wird vom Spiel als Heilung ausgewertet (Ereignis PickupCollected)
    } else {
        Log::warn(LogCategory::World, "Unbekannter Aufsammel-Typ '{}' in Raum {}", t, roomId_);
    }
}

void Room::updatePickups(const RoomActor* player, EventBus& events) {
    if (!player) return;
    Vector3 p = player->body->position;
    for (RoomPickup& pk : pickups_) {
        if (pk.collected) continue;
        float dx = pk.def.position.x - p.x, dz = pk.def.position.z - p.z;
        if (std::sqrt(dx * dx + dz * dz) > kPickupRadius) continue;
        if (pk.def.position.y < p.y - 0.6f || pk.def.position.y > p.y + player->body->height + 0.6f) continue;
        pk.collected = true;
        if (pk.persistent) state_.collected.insert(DungeonState::key(roomId_, pk.def.id));
        applyPickupEffect(pk, events);
        events.emit(PickupCollected{pk.def.type, pk.def.id});
    }
}

void Room::addPickup(const std::string& type, Vector3 position) {
    RoomPickup p;
    p.def.type = type;
    p.def.id = "dyn" + std::to_string(++dynamicPickupCounter_);
    p.def.position = position;
    p.persistent = false;
    pickups_.push_back(std::move(p));
}

void Room::addPersistentPickup(const std::string& type, const std::string& id, Vector3 position) {
    if (state_.collected.count(DungeonState::key(roomId_, id))) return;
    for (const RoomPickup& p : pickups_) {
        if (p.def.id == id) return;  // liegt schon da
    }
    RoomPickup p;
    p.def.type = type;
    p.def.id = id;
    p.def.position = position;
    p.persistent = true;
    pickups_.push_back(std::move(p));
}

// ---------------------------------------------------------------- Arena

void Room::spawnWave(int wave) {
    const LevelEncounter& enc = *level_.encounter;
    size_t k = 0;
    size_t total = 0;
    for (const WaveEntry& e : enc.waves[(size_t)wave]) total += (size_t)e.count;
    for (const WaveEntry& e : enc.waves[(size_t)wave]) {
        for (int c = 0; c < e.count; c++, k++) {
            Vector3 pos = enc.spawnPoints[k % enc.spawnPoints.size()];
            // Mehr Gegner als Startpunkte: leicht versetzen, damit sie nicht ineinander stehen
            if (total > enc.spawnPoints.size()) {
                float a = (float)k * 2.4f;
                pos.x += std::cos(a) * 1.3f * (float)(k / enc.spawnPoints.size());
                pos.z += std::sin(a) * 1.3f * (float)(k / enc.spawnPoints.size());
            }
            spawnRequests_.push_back({e.type, pos});
        }
    }
}

void Room::updateEncounter(float dt, const RoomActor* player, int aliveEnemies, EventBus& events) {
    if (encounterPhase_ == EncounterPhase::None || encounterPhase_ == EncounterPhase::Cleared) return;

    switch (encounterPhase_) {
        case EncounterPhase::Waiting: {
            bool inside = false;
            if (player) {
                float hx = (level_.shellSize.x > 0 ? level_.shellSize.x : level_.groundSize.x) * 0.5f - 1.0f;
                float hz = (level_.shellSize.y > 0 ? level_.shellSize.y : level_.groundSize.y) * 0.5f - 1.0f;
                Vector3 p = player->body->position;
                inside = std::fabs(p.x) < hx && std::fabs(p.z) < hz;
            }
            insideTimer_ = inside ? insideTimer_ + dt : 0.0f;
            if (insideTimer_ >= kEncounterEnterDelay) {
                encounterPhase_ = EncounterPhase::Countdown;
                encounterTimer_ = kCountdownTime;
                events.emit(EncounterStarted{});
            }
            break;
        }
        case EncounterPhase::Countdown:
            encounterTimer_ -= dt;
            if (encounterTimer_ <= 0.0f) {
                waveIndex_ = 0;
                spawnWave(0);
                encounterPhase_ = EncounterPhase::Fighting;
                events.emit(EncounterWaveStarted{1, totalWaves()});
            }
            break;
        case EncounterPhase::Fighting:
            if (spawnRequests_.empty() && aliveEnemies == 0) {
                if (waveIndex_ + 1 >= totalWaves()) {
                    encounterPhase_ = EncounterPhase::Cleared;
                    state_.clearedRooms.insert(roomId_);
                    encounterClearedFlag_ = true;
                    events.emit(EncounterCleared{});
                } else {
                    encounterPhase_ = EncounterPhase::Between;
                    encounterTimer_ = kBetweenWavesTime;
                }
            }
            break;
        case EncounterPhase::Between:
            encounterTimer_ -= dt;
            if (encounterTimer_ <= 0.0f) {
                waveIndex_++;
                spawnWave(waveIndex_);
                encounterPhase_ = EncounterPhase::Fighting;
                events.emit(EncounterWaveStarted{waveIndex_ + 1, totalWaves()});
            }
            break;
        default: break;
    }
}

std::vector<SpawnRequest> Room::takeSpawnRequests() {
    std::vector<SpawnRequest> out;
    out.swap(spawnRequests_);
    return out;
}

bool Room::takeEncounterCleared() {
    bool f = encounterClearedFlag_;
    encounterClearedFlag_ = false;
    return f;
}

void Room::setBossDefeated(EventBus& events) {
    bossDefeated_ = true;
    bossFight_ = false;
    state_.clearedRooms.insert(roomId_);
    if (level_.boss) {
        state_.defeatedBosses.insert(level_.boss->type);
        events.emit(BossDefeated{level_.boss->type});
    }
}

// ---------------------------------------------------------------- Gefahren

const LevelHazard* Room::hazardAt(Vector3 feet, float radius, float height) const {
    for (const LevelHazard& h : level_.hazards) {
        const LevelBox& box = level_.boxes[h.boxIndex];
        if (feet.y > box.maxY() + 0.15f || feet.y + height < box.minY()) continue;
        if (circleOverlapsRect(box.minX(), box.maxX(), box.minZ(), box.maxZ(), feet.x, feet.z, radius * 0.8f)) return &h;
    }
    return nullptr;
}

}  // namespace aldoria
