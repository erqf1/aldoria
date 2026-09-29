#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio/sound_bank.h"
#include "combat/player_modifiers.h"
#include "combat/projectile.h"
#include "core/arena_director.h"
#include "core/dungeon_def.h"
#include "core/dungeon_state.h"
#include "core/game_context.h"
#include "core/save_manager.h"
#include "core/scene.h"
#include "debug/debug_overlay.h"
#include "entities/boss.h"
#include "entities/enemy.h"
#include "entities/enemy_def.h"
#include "entities/player.h"
#include "render/camera_rig.h"
#include "render/particles.h"
#include "ui/dungeon_map.h"
#include "ui/menu.h"
#include "ui/settings_menu.h"
#include "world/room.h"

namespace aldoria {

enum class StartMode { NewGame, Continue, Arena, BossRush };

// Der Dungeon-Durchlauf: Räume laden und wechseln, Kämpfe, Bosse, Upgrades, Speichern, Tod und Ende.
class GameScene : public Scene {
public:
    GameScene(GameContext& ctx, StartMode mode);
    ~GameScene() override;

    void update(float dt) override;
    void draw() override;
    bool debugCommand(const std::string& name, const std::vector<std::string>& args) override;
    std::unique_ptr<Scene> takeNext() override;

    // Für Tests
    const DungeonState& state() const { return state_; }
    const Room* room() const { return room_.get(); }
    Player& player() { return player_; }

private:
    enum class Phase { Playing, Paused, Segen, Dying, Complete, ArenaOver };
    enum class Fade { None, Out, In };

    struct FloatingText {
        Vector3 position;
        std::string text;
        Color color;
        float age = 0.0f;
    };
    // Einschlag mit Vorwarnung (Meteore): roter Kreis am Boden, nach `delay` Sekunden Schaden
    struct Blast {
        Vector3 center{0, 0, 0};
        float radius = 2.0f;
        float delay = 1.0f;
        float total = 1.0f;
        float damage = 20.0f;
        Color color{255, 90, 50, 255};
    };
    struct Shockwave {
        Vector3 center;
        float radius = 0.5f;
        float speed = 9.0f;
        float maxRadius = 15.0f;
        float damage = 16.0f;
        bool hit = false;
    };
    struct PendingLoad {
        std::string room;
        std::string door;
        std::optional<Vector3> position;
        std::optional<float> yaw;
        bool respawn = false;
    };

    static constexpr float kFloatingTextLife = 0.9f;
    static constexpr float kRespawnDelay = 2.6f;
    static constexpr float kFadeOutTime = 0.25f;
    static constexpr float kFadeInTime = 0.3f;

    // ---- Laden und Zustand
    bool loadRoom(const std::string& roomId, const std::string& arrivalDoor, std::optional<Vector3> position, std::optional<float> yaw);
    void beginTransition(PendingLoad load);
    void syncPlayerFromState();
    const EnemyDef* enemyDef(const std::string& type);
    const BossDef* bossDef(const std::string& type);
    Enemy* spawnEnemy(const std::string& type, Vector3 position, float delay);
    void subscribeEvents();
    bool saveGame();
    void activateCheckpoint();
    void respawnAfterDeath();

    // ---- Spielablauf
    void updatePlaying(float dt);
    void updatePaused();
    void updateSegen();
    void updateDying(float dt);
    void updateFade(float dt);
    void updateEnemies(float dt);
    void updateArena(float dt);
    void spawnArenaBoss(const std::string& type);
    void loadArenaBest();
    void saveArenaBest();
    void updateBoss(float dt);
    void updateShockwaves(float dt);
    void updateBlasts(float dt);
    void updateHazardsAndFalls(float dt);
    void updateEffectsAndTexts(float dt);
    void spawnLavaEmbers(float dt);
    void resolveCombat();
    void resolveProjectileHits();
    void processBossEvents();
    void separateEnemies();
    void onEnemyKilled(Enemy& e);
    void startBossFight();
    void tryDrinkFlask();
    void openSegenChoice(int shardCost = 0);
    bool segenAvailable() const;
    int shrineSegenCost() const { return 10 + 3 * (int)state_.segen.size(); }
    void openPause();
    void openMap(bool fromPlaying);
    void closePause();
    void spawnSpark(Vector3 origin, Vector3 direction);
    float damagePlayer(float amount, Vector3 knockbackDirection, float knockback);
    float dealPlayerDamageRoll(float base, bool& crit);
    std::optional<Vector3> findAssistTarget() const;
    int aliveEnemyCount() const;
    MusicKind currentMood() const;

    // ---- Anzeige
    void addFloatingText(Vector3 position, std::string text, Color color);
    void showToast(std::string text, float seconds = 3.0f);
    void showBanner(std::string title, std::string subtitle, float seconds = 2.6f);
    void setMouseCaptured(bool captured);
    void drawWorld();
    void drawHudAndOverlays();
    void buildMenus();
    bool loadDungeon(const std::string& id);
    void advanceToNextDungeon();
    void rebuildCompleteMenu();
    float bannerAlpha() const;

    GameContext& ctx_;
    DungeonDef dungeon_;
    DungeonState state_;
    SaveManager saves_;
    SegenLibrary segen_;

    std::unique_ptr<Room> room_;
    Player player_;
    CameraRig camera_;
    DebugOverlay debug_;
    std::unordered_map<std::string, EnemyDef> enemyDefs_;   // Zeiger auf Einträge bleiben stabil
    std::unordered_map<std::string, BossDef> bossDefs_;
    std::vector<Enemy> enemies_;
    std::unique_ptr<Boss> boss_;
    bool bossFightActive_ = false;
    bool bossRewardGiven_ = false;
    ProjectileSystem projectiles_;
    Particles particles_;
    std::vector<Shockwave> shockwaves_;
    std::vector<Blast> blasts_;
    std::vector<FloatingText> floatingTexts_;

    Phase phase_ = Phase::Playing;
    Fade fade_ = Fade::None;
    float fadeTimer_ = 0.0f;
    std::optional<PendingLoad> pending_;

    // Segenwahl
    std::vector<const SegenDef*> segenChoices_;
    int segenSelected_ = 0;
    int segenCost_ = 0;   // > 0: am Ruheplatz für Splitter gekauft

    // Pause
    Menu pauseMenu_;
    std::unique_ptr<SettingsScreen> settingsScreen_;
    std::function<void(int)> settingsSound_;
    bool inSettings_ = false;
    bool showMap_ = false;
    bool mapFromPlaying_ = false;
    DungeonMap map_;
    std::string mapDungeonId_;

    std::vector<SubscriptionId> subscriptions_;
    bool mouseCaptured_ = false;
    bool skipLook_ = true;
    bool wasFocused_ = true;
    float deathTimer_ = 0.0f;
    float hurtFlash_ = 0.0f;   // 1 bei einem Treffer, klingt ab (roter Bildschirmrand)
    float time_ = 0.0f;
    float staticEnemyCount_ = 0.0f;
    int staticEnemiesKilled_ = 0;
    int staticEnemiesTotal_ = 0;

    // Sicherer Wiedereinstiegspunkt nach Abstürzen
    Vector3 safePosition_{0, 0, 0};
    float safeTimer_ = 0.0f;

    std::string toast_;
    float toastTimer_ = 0.0f;
    std::string bannerTitle_, bannerSubtitle_;
    float bannerTimer_ = 0.0f, bannerDuration_ = 0.0f;

    // Endlos-Arena
    bool arenaMode_ = false;
    bool bossRush_ = false;
    std::unique_ptr<ArenaDirector> director_;
    int arenaBest_ = 0;
    Menu arenaOverMenu_;
    Menu completeMenu_;
    std::unique_ptr<Scene> nextScene_;

    bool quitToTitle_ = false;
    bool complete_ = false;
    bool godMode_ = false;
    std::optional<Camera3D> debugCamera_;  // Testbefehl "cam": feste Kamera statt der Verfolgerkamera
};

}  // namespace aldoria
