#include "core/arena_director.h"

#include <algorithm>

namespace aldoria {

ArenaDirector::ArenaDirector(unsigned seed, std::vector<Vector3> spawnPoints) : rng_(seed), spawnPoints_(std::move(spawnPoints)) {
    if (spawnPoints_.empty()) spawnPoints_.push_back({0, 0, 0});
}

int ArenaDirector::enemyCost(const std::string& type) {
    if (type == "forest_imp") return 1;
    if (type == "imp_archer") return 2;
    if (type == "leaper") return 2;
    if (type == "brute" || type == "magma_brute" || type == "gale_brute") return 5;
    if (type == "ember_archer" || type == "cinder_leaper" || type == "storm_archer" || type == "gale_leaper") return 2;
    return 1;
}

std::vector<std::string> ArenaDirector::composeWave(int wave, std::mt19937& rng) {
    struct Option {
        const char* type;
        int weight;
    };
    std::vector<Option> options = {{"forest_imp", 6}};
    if (wave >= 2) options.push_back({"imp_archer", 3});
    if (wave >= 3) options.push_back({"leaper", 3});
    if (wave >= 5) options.push_back({"brute", 2});
    // Ab Welle 8 mischen sich Gegner aus der Glutschmiede unter die Kobolde
    if (wave >= 8) {
        options.push_back({"ember_imp", 4});
        options.push_back({"ember_archer", 2});
        options.push_back({"cinder_leaper", 2});
    }
    if (wave >= 12) options.push_back({"magma_brute", 2});
    // Ab Welle 16 kommen die Sturmgegner aus dem Himmelsturm dazu
    if (wave >= 16) {
        options.push_back({"wind_imp", 4});
        options.push_back({"storm_archer", 2});
        options.push_back({"gale_leaper", 2});
    }
    if (wave >= 20) options.push_back({"gale_brute", 2});

    float budget = budgetForWave(wave);
    std::vector<std::string> out;
    int guard = 0;
    while (budget > 0.5f && out.size() < 14 && guard++ < 100) {
        // Nur Arten wählen, die noch ins Budget passen
        std::vector<Option> affordable;
        int total = 0;
        for (const Option& o : options) {
            if ((float)enemyCost(o.type) <= budget) {
                affordable.push_back(o);
                total += o.weight;
            }
        }
        if (affordable.empty()) break;
        int pick = (int)(rng() % (unsigned)total);
        for (const Option& o : affordable) {
            pick -= o.weight;
            if (pick < 0) {
                out.push_back(o.type);
                budget -= (float)enemyCost(o.type);
                break;
            }
        }
    }
    return out;
}

Vector3 ArenaDirector::pickSpawnPoint() { return spawnPoints_[rng_() % spawnPoints_.size()]; }

ArenaDirector::Update ArenaDirector::update(float dt, int aliveEnemies, bool bossAlive) {
    Update u;
    if (dt <= 0.0f) return u;

    if (!waveActive_) {
        timer_ -= dt;
        if (timer_ > 0.0f) return u;
        wave_++;
        waveActive_ = true;
        u.waveStarted = true;
        bossWave_ = bossRush_ || wave_ % kBossEvery == 0;
        if (bossWave_) {
            // Reihum: Kobold-König, Schmiedegolem, Sturmwächter
            int index = bossRush_ ? wave_ : wave_ / kBossEvery;
            static const char* const kBosses[] = {"kobold_king", "schmiedegolem", "sturmwaechter"};
            u.boss = kBosses[(index - 1) % 3];
        } else {
            for (const std::string& type : composeWave(wave_, rng_)) {
                Vector3 p = pickSpawnPoint();
                // Nicht alle auf denselben Punkt stellen
                p.x += (float)((int)(rng_() % 21) - 10) * 0.12f;
                p.z += (float)((int)(rng_() % 21) - 10) * 0.12f;
                u.spawns.push_back({type, p});
            }
        }
        return u;
    }

    // Welle läuft: erledigt, sobald nichts mehr lebt (nach dem Auftauchen der Gegner)
    if (aliveEnemies == 0 && !bossAlive) {
        waveActive_ = false;
        timer_ = 3.0f;
        u.waveCleared = true;
        u.offerSegen = wave_ % kSegenEvery == 0 || bossWave_;
    }
    return u;
}

}  // namespace aldoria
