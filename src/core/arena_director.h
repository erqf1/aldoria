#pragma once

#include <optional>
#include <random>
#include <string>
#include <vector>

#include "world/room.h"

namespace aldoria {

// Steuert den Endlos-Arena-Modus: Wellen wachsen mit einem Punktebudget, neue Gegnerarten kommen nach und nach hinzu,
// alle drei Wellen gibt es einen Segen, alle zehn Wellen einen Boss. Reine Logik, ohne Grafik.
class ArenaDirector {
public:
    static constexpr int kBossEvery = 10;
    static constexpr int kSegenEvery = 3;

    explicit ArenaDirector(unsigned seed = 1, std::vector<Vector3> spawnPoints = {{0, 0, 0}});

    struct Update {
        std::vector<SpawnRequest> spawns;
        std::optional<std::string> boss;   // in dieser Welle kommt ein Boss (statt normaler Gegner)
        bool waveStarted = false;
        bool waveCleared = false;
        bool offerSegen = false;           // nach dieser Welle gibt es einen Segen
    };

    // `aliveEnemies`: alle lebenden Gegner (auch auftauchende); `bossAlive`: der Boss der Welle lebt noch
    Update update(float dt, int aliveEnemies, bool bossAlive = false);

    // Boss-Rush: jede Welle ist ein Boss (Kobold-König, Schmiedegolem, Sturmwächter, dann wieder von vorn), nach jedem gibt es einen Segen
    void setBossRush(bool on) { bossRush_ = on; }
    bool bossRush() const { return bossRush_; }

    int wave() const { return wave_; }
    int waveKills() const { return kills_; }
    void addKill() { kills_++; }
    bool waveActive() const { return waveActive_; }

    // Zusammenstellung einer Welle (Gegnertypen), unabhängig vom Ablauf testbar
    static std::vector<std::string> composeWave(int wave, std::mt19937& rng);
    static int enemyCost(const std::string& type);
    static float budgetForWave(int wave) { return 2.0f + (float)wave * 1.6f; }

private:
    Vector3 pickSpawnPoint();

    std::mt19937 rng_;
    std::vector<Vector3> spawnPoints_;
    int wave_ = 0;
    int kills_ = 0;
    float timer_ = 2.5f;       // Wartezeit bis zur nächsten Welle
    bool waveActive_ = false;
    bool bossWave_ = false;
    bool bossRush_ = false;
    bool spawnedBossFlag_ = false;
};

}  // namespace aldoria
