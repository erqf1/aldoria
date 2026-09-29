#pragma once

#include <optional>
#include <string>
#include <vector>

#include "combat/melee.h"
#include "core/dungeon_state.h"
#include "core/event_bus.h"
#include "world/kinematic_body.h"
#include "world/level.h"

namespace aldoria {

// Wer auf Platten steht und von Plattformen getragen wird
struct RoomActor {
    KinematicBody* body = nullptr;
    bool isPlayer = false;
    bool alive = true;
};

struct SpawnRequest {
    std::string type;
    Vector3 position{0, 0, 0};
};

struct RoomTransition {
    std::string room;
    std::string door;
};

enum class EncounterPhase { None, Waiting, Countdown, Fighting, Between, Cleared };

// Ein aufsammelbares Ding im Raum. Persistente Dinge (Schlüssel, Splitter ...) merkt sich der DungeonState.
struct RoomPickup {
    LevelPickup def;
    bool collected = false;
    bool persistent = true;
};

// Laufzeit eines Raums: bewegt Plattformen, wertet Schalter und Türen aus, sammelt Dinge ein und steuert
// Arena-Kämpfe. Reine Logik ohne Grafik. Der Raum verändert seine eigene Kopie des Levels (Türen öffnen sich,
// Plattformen fahren), die Kollision der Figuren nutzt immer `level()`.
class Room {
public:
    Room(Level level, DungeonState& state, std::string roomId);

    const Level& level() const { return level_; }
    const std::string& id() const { return roomId_; }

    // Einmal pro Frame. `aliveEnemies` zählt alle lebenden Gegner (auch solche, die gerade auftauchen).
    void update(float dt, std::vector<RoomActor>& actors, int aliveEnemies, EventBus& events);

    // Schwertschlag oder Funken aktivieren Kristalle
    void hitCrystals(const MeleeQuery& query, EventBus& events);
    void hitCrystalsAt(Vector3 position, float radius, EventBus& events);

    // Einmal abholbare Ergebnisse
    std::optional<RoomTransition> takeTransition();
    std::vector<SpawnRequest> takeSpawnRequests();
    bool takeEncounterCleared();

    // Abfragen
    const LevelHazard* hazardAt(Vector3 feet, float radius, float height) const;
    bool doorOpen(size_t index) const { return index < doors_.size() && doors_[index].open; }
    bool switchActive(const std::string& id) const;
    bool switchActive(size_t index) const { return index < switches_.size() && switches_[index].active; }
    const std::vector<RoomPickup>& pickups() const { return pickups_; }
    EncounterPhase encounterPhase() const { return encounterPhase_; }
    int currentWave() const { return waveIndex_; }
    int totalWaves() const { return level_.encounter ? (int)level_.encounter->waves.size() : 0; }
    bool inCombat() const;
    bool bossDefeated() const { return bossDefeated_; }
    bool cleared() const { return state_.isCleared(roomId_); }
    float time() const { return time_; }

    void setBossFight(bool active) { bossFight_ = active; }
    void setBossDefeated(EventBus& events);
    void markCleared() { state_.clearedRooms.insert(roomId_); }
    // Nicht gespeichertes Ding (z. B. Herz aus einem Gegner)
    void addPickup(const std::string& type, Vector3 position);
    // Gespeichertes Ding mit fester ID (z. B. Belohnung nach einem Boss); nichts passiert, wenn schon eingesammelt
    void addPersistentPickup(const std::string& type, const std::string& id, Vector3 position);

private:
    struct PlatformRuntime {
        size_t next = 1;
        int direction = 1;
        float pauseTimer = 0.0f;
        // Bröckeln
        enum class Crumble { Solid, Warning, Falling, Gone } crumble = Crumble::Solid;
        float timer = 0.0f;
        Vector3 home{0, 0, 0};
    };
    struct SwitchRuntime {
        bool active = false;
        bool latched = false;
        float holdTimer = 0.0f;
    };
    struct DoorRuntime {
        bool open = false;
    };

    void updatePlatforms(float dt, std::vector<RoomActor>& actors, EventBus& events);
    void updateSwitches(float dt, const std::vector<RoomActor>& actors, EventBus& events);
    void updateDoors(const RoomActor* player, EventBus& events);
    void updatePickups(const RoomActor* player, EventBus& events);
    void updateEncounter(float dt, const RoomActor* player, int aliveEnemies, EventBus& events);
    void applyDoorState(size_t index, bool open, bool notify, EventBus& events);
    void latchSwitch(size_t index, EventBus& events);
    void spawnWave(int wave);
    void applyPickupEffect(const RoomPickup& p, EventBus& events);
    bool desiredDoorOpen(size_t index, const RoomActor* player, EventBus& events);

    Level level_;
    DungeonState& state_;
    std::string roomId_;

    std::vector<PlatformRuntime> platforms_;
    std::vector<SwitchRuntime> switches_;
    std::vector<DoorRuntime> doors_;
    std::vector<RoomPickup> pickups_;

    EncounterPhase encounterPhase_ = EncounterPhase::None;
    float encounterTimer_ = 0.0f;
    float insideTimer_ = 0.0f;
    int waveIndex_ = -1;
    bool encounterClearedFlag_ = false;
    std::vector<SpawnRequest> spawnRequests_;

    std::optional<RoomTransition> transition_;
    bool bossFight_ = false;
    bool bossDefeated_ = false;
    bool firstUpdate_ = true;
    float time_ = 0.0f;
    int dynamicPickupCounter_ = 0;
};

}  // namespace aldoria
