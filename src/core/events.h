#pragma once

#include <string>

namespace aldoria {

// Ereignisse, die über den EventBus laufen. Neue Systeme (Ton, UI, Erfolge) hören hier mit,
// ohne dass Spieler-Code etwas von ihnen wissen muss.
struct LevelLoaded {
    std::string levelId;
};
struct PlayerJumped {};
struct PlayerAirJumped {};   // zweiter Sprung in der Luft (Fähigkeit Doppelsprung)
struct PlayerLanded {
    float impactSpeed;
};
struct PlayerRespawned {};
struct PlayerAttacked {
    int comboStep;
};
struct PlayerDodged {};
struct PlayerDamaged {
    float amount;
    float remaining;
};
struct PlayerDied {};
struct EnemyDamaged {
    std::string type;
    float amount;
};
struct EnemyDied {
    std::string type;
};
struct PlayerHealed {
    float amount;
};
struct PlayerDashed {};
struct PlayerCast {};  // Funkenwurf abgeschossen
struct RoomEntered {
    std::string roomId;
};
struct DoorOpened {
    std::string roomId;
    std::string doorId;
};
struct DoorUnlocked {
    std::string roomId;
    std::string doorId;
};
struct SwitchToggled {
    std::string switchId;
    bool active;
};
struct PickupCollected {
    std::string type;
    std::string id;
};
struct PlatformCrumbled {
    std::string platformId;
};
struct EncounterStarted {};
struct EncounterWaveStarted {
    int wave;
    int totalWaves;
};
struct EncounterCleared {};
struct BossPhaseChanged {
    std::string bossType;
    int phase;
};
struct BossDefeated {
    std::string bossType;
};
struct CheckpointActivated {
    std::string roomId;
};
struct SegenChosen {
    std::string id;
};
struct AbilityUnlocked {
    std::string ability;
};
struct BossRoar {};
struct BossStomp {};
struct BossVolleyFired {};
struct EnemyShot {};

}  // namespace aldoria
