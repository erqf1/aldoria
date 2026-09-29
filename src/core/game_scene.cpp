#include "core/game_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>

#include "core/events.h"
#include "core/file_util.h"
#include "core/log.h"
#include "core/math_util.h"
#include "core/paths.h"
#include "core/title_scene.h"
#include "raymath.h"
#include "render/room_renderer.h"
#include "ui/hud.h"
#include "ui/settings_menu.h"

namespace aldoria {
namespace {

constexpr float kAssistRange = 3.5f;
constexpr float kBossTriggerDistance = 15.0f;

// Horizontale Richtung von `from` nach `to`; bei praktisch gleicher Position die Blickrichtung
Vector3 horizontalDirection(Vector3 from, Vector3 to, float fallbackYaw) {
    Vector3 d{to.x - from.x, 0.0f, to.z - from.z};
    float len = Vector3Length(d);
    if (len < 1e-4f) return {std::sin(fallbackYaw), 0.0f, std::cos(fallbackYaw)};
    return Vector3Scale(d, 1.0f / len);
}

float randomUnit() { return (float)GetRandomValue(0, 10000) / 10000.0f; }

const char* roomTypeLabel(const std::string& type) {
    if (type == "platform") return "Geschicklichkeit";
    if (type == "arena") return "Kampfarena";
    if (type == "puzzle") return "Rätselraum";
    if (type == "boss") return "Bossraum";
    if (type == "rest") return "Ruheplatz";
    if (type == "treasure") return "Schatzkammer";
    if (type == "hub") return "Wegkreuzung";
    return "";
}

Color pickupColor(const std::string& type) {
    if (type == "small_key") return {242, 204, 80, 255};
    if (type == "boss_key") return {200, 100, 240, 255};
    if (type == "shard") return {110, 210, 255, 255};
    if (type == "heart" || type == "health_drop") return {236, 70, 90, 255};
    if (type == "flask" || type == "potion") return {110, 224, 140, 255};
    return {255, 255, 255, 255};
}

}  // namespace

// ---------------------------------------------------------------- Aufbau

GameScene::GameScene(GameContext& ctx, StartMode mode) : ctx_(ctx), saves_(savesDir()) {
    bossRush_ = mode == StartMode::BossRush;
    arenaMode_ = mode == StartMode::Arena || bossRush_;
    std::string error;
    std::optional<DungeonState> saved;
    if (mode == StartMode::Continue) {
        saved = saves_.load("slot1", error);
        if (!saved) Log::warn(LogCategory::Save, "Kein Spielstand gefunden, starte neu");
    }
    std::string dungeonId = saved && !saved->dungeonId.empty() ? saved->dungeonId : "wurzelhallen";
    if (!loadDungeon(dungeonId)) {
        if (dungeonId != "wurzelhallen") {
            saved.reset();
            dungeonId = "wurzelhallen";
        }
        if (!loadDungeon(dungeonId)) {
            dungeon_ = DungeonDef{};
            dungeon_.id = "fallback";
            dungeon_.name = "Notfall";
            dungeon_.startRoom = "dev_playground";
        }
    }
    segen_ = SegenLibrary::loadFromFile(dataPath("segen.json"));
    player_.configure(PlayerConfig::loadFromFile(dataPath("config/player.json")));
    WeaponDef weapon = WeaponDef::loadFromFile(dataPath("weapons/sword.json"));
    if (weapon.valid()) player_.setWeapon(std::move(weapon));

    bool continued = false;
    if (saved && saved->dungeonId == dungeon_.id) {
        state_ = *saved;
        continued = true;
        Log::info(LogCategory::Save, "Spielstand geladen (Raum {}, {} Splitter)", state_.restRoom, state_.shards);
    }
    if (!continued) {
        state_ = DungeonState{};
        state_.dungeonId = dungeon_.id;
    }

    debug_.visible = false;  // Entwickleranzeige nur auf Wunsch (F1)
    subscribeEvents();
    buildMenus();
    syncPlayerFromState();
    player_.spawn({0, 0, 0});
    state_.flaskCharges = continued ? state_.flaskMax : state_.flaskCharges;

    bool loaded = false;
    if (arenaMode_) {
        loaded = loadRoom("arena_endless", "", std::nullopt, std::nullopt);
        if (loaded) {
            director_ = std::make_unique<ArenaDirector>((unsigned)GetRandomValue(1, 1000000), room_->level().arenaSpawnPoints);
            director_->setBossRush(bossRush_);
            loadArenaBest();
            if (bossRush_) showBanner("Boss-Rush", "Wie viele Bosse besiegst du?", 3.0f);
            else showBanner("Endlose Arena", "Wie viele Wellen überlebst du?", 3.0f);
        }
    } else {
        if (continued && !state_.restRoom.empty()) loaded = loadRoom(state_.restRoom, "", state_.restPosition, std::nullopt);
        if (!loaded) loaded = loadRoom(dungeon_.startRoom, dungeon_.startDoor, std::nullopt, std::nullopt);
    }
    if (!loaded) showToast("Der Startraum konnte nicht geladen werden, siehe aldoria.log", 8.0f);

    if (!ctx_.input.injectionMode()) setMouseCaptured(true);
    else mouseCaptured_ = true;
}

GameScene::~GameScene() {
    for (SubscriptionId id : subscriptions_) ctx_.events.unsubscribe(id);
    if (!ctx_.input.injectionMode()) EnableCursor();
}

void GameScene::subscribeEvents() {
    auto& ev = ctx_.events;
    subscriptions_.push_back(ev.subscribe<PickupCollected>([this](const PickupCollected& e) {
        syncPlayerFromState();
        Vector3 p = player_.position();
        p.y += 1.0f;
        particles_.sparkle(p, 16, pickupColor(e.type), 0.55f);
        if (e.type == "small_key") showToast("Kleiner Schlüssel gefunden");
        else if (e.type == "boss_key") showBanner("Boss-Schlüssel!", "Er öffnet die Tür zum Herrscher dieses Dungeons.");
        else if (e.type == "heart") {
            player_.healBy(999.0f, ctx_.events);
            showToast("Herzcontainer: +10 maximale Lebenspunkte");
        } else if (e.type == "flask") showToast("Größere Flasche: eine Ladung mehr");
        else if (e.type == "potion") showToast("Heiltrank aufgefüllt");
        else if (e.type == "health_drop") player_.healBy(15.0f, ctx_.events);
        else if (e.type == "shard") addFloatingText({p.x, p.y + 0.5f, p.z}, "+1", Color{130, 220, 255, 255});
    }));
    subscriptions_.push_back(ev.subscribe<AbilityUnlocked>([this](const AbilityUnlocked& e) {
        syncPlayerFromState();
        if (e.ability == "dash") showBanner("Windstiefel gefunden!", "Alt oder V: kurzer Sturm nach vorn, auch in der Luft.", 4.5f);
        else if (e.ability == "double_jump") showBanner("Sturmfedern gefunden!", "Leertaste in der Luft: ein zweiter Sprung.", 4.5f);
    }));
    subscriptions_.push_back(ev.subscribe<DoorUnlocked>([this](const DoorUnlocked&) { showToast("Tür aufgeschlossen"); }));
    subscriptions_.push_back(ev.subscribe<SwitchToggled>([this](const SwitchToggled& e) {
        if (e.active) showToast("Schalter aktiviert");
    }));
    subscriptions_.push_back(ev.subscribe<EncounterStarted>([this](const EncounterStarted&) {
        showBanner("Kampf!", "Besiege alle Gegner, um weiterzukommen.", 2.2f);
    }));
    subscriptions_.push_back(ev.subscribe<EncounterWaveStarted>([this](const EncounterWaveStarted& e) {
        if (e.totalWaves > 0 && (e.wave > 1 || e.totalWaves > 1)) showBanner(std::format("Welle {} von {}", e.wave, e.totalWaves), "", 1.8f);
    }));
    subscriptions_.push_back(ev.subscribe<EncounterCleared>([this](const EncounterCleared&) { showBanner("Geschafft!", "", 1.8f); }));
    subscriptions_.push_back(ev.subscribe<PlayerLanded>([this](const PlayerLanded& e) {
        if (e.impactSpeed > 6.0f) particles_.dustRing({player_.position().x, player_.position().y + 0.05f, player_.position().z}, 0.5f, 10, Color{190, 180, 160, 200});
    }));
    subscriptions_.push_back(ev.subscribe<PlayerJumped>([this](const PlayerJumped&) {
        particles_.dustRing({player_.position().x, player_.position().y + 0.05f, player_.position().z}, 0.35f, 6, Color{190, 180, 160, 160});
    }));
    subscriptions_.push_back(ev.subscribe<PlayerAirJumped>([this](const PlayerAirJumped&) {
        Vector3 p = player_.position();
        particles_.dustRing({p.x, p.y + 0.1f, p.z}, 0.4f, 10, Color{190, 235, 255, 200});
        particles_.burst({p.x, p.y + 0.2f, p.z}, 8, Color{220, 245, 255, 220}, 2.5f, 0.08f, 0.35f, 0.0f);
    }));
    subscriptions_.push_back(ev.subscribe<PlayerDashed>([this](const PlayerDashed&) {
        particles_.burst({player_.position().x, player_.position().y + 0.9f, player_.position().z}, 14, Color{150, 210, 255, 220}, 3.0f, 0.1f, 0.4f, 0.0f);
    }));
    subscriptions_.push_back(ev.subscribe<BossPhaseChanged>([this](const BossPhaseChanged& e) {
        const BossDef* def = boss_ ? &boss_->def() : nullptr;
        std::string text = def ? (e.phase == 2 ? def->phase2Text : def->phase3Text) : std::string();
        showBanner(std::format("Phase {}", e.phase), text, 2.4f);
    }));

    // Wichtige Ereignisse ins Protokoll, damit sich Abläufe (auch in Testläufen) nachvollziehen lassen
    auto logC = [](const std::string& s) { Log::info(LogCategory::Player, "EREIGNIS {}", s); };
    subscriptions_.push_back(ev.subscribe<DoorOpened>([logC](const DoorOpened& e) { logC("Tür geöffnet: " + e.roomId + ":" + e.doorId); }));
    subscriptions_.push_back(ev.subscribe<DoorUnlocked>([logC](const DoorUnlocked& e) { logC("Tür aufgeschlossen: " + e.roomId + ":" + e.doorId); }));
    subscriptions_.push_back(ev.subscribe<SwitchToggled>([logC](const SwitchToggled& e) { logC("Schalter " + e.switchId + (e.active ? " an" : " aus")); }));
    subscriptions_.push_back(ev.subscribe<PickupCollected>([logC](const PickupCollected& e) { logC("Eingesammelt: " + e.type + " (" + e.id + ")"); }));
    subscriptions_.push_back(ev.subscribe<EncounterStarted>([logC](const EncounterStarted&) { logC("Arena-Kampf beginnt"); }));
    subscriptions_.push_back(ev.subscribe<EncounterWaveStarted>([logC](const EncounterWaveStarted& e) { logC(std::format("Welle {}/{}", e.wave, e.totalWaves)); }));
    subscriptions_.push_back(ev.subscribe<EncounterCleared>([logC](const EncounterCleared&) { logC("Arena gesäubert"); }));
    subscriptions_.push_back(ev.subscribe<BossPhaseChanged>([logC](const BossPhaseChanged& e) { logC(std::format("Boss Phase {}", e.phase)); }));
    subscriptions_.push_back(ev.subscribe<BossDefeated>([logC](const BossDefeated& e) { logC("Boss besiegt: " + e.bossType); }));
    subscriptions_.push_back(ev.subscribe<CheckpointActivated>([logC](const CheckpointActivated& e) { logC("Ruheplatz aktiviert: " + e.roomId); }));
    subscriptions_.push_back(ev.subscribe<SegenChosen>([logC](const SegenChosen& e) { logC("Segen gewählt: " + e.id); }));
    subscriptions_.push_back(ev.subscribe<PlayerDied>([logC](const PlayerDied&) { logC("Spieler gestorben"); }));
    subscriptions_.push_back(ev.subscribe<AbilityUnlocked>([logC](const AbilityUnlocked& e) { logC("Fähigkeit: " + e.ability); }));
}

void GameScene::buildMenus() {
    auto saveSettingsNow = [this]() { saveSettings(ctx_.settings, (std::filesystem::path(savesDir()) / "settings.json").string()); };

    std::vector<MenuItem> pause;
    {
        MenuItem resume;
        resume.label = "Weiter";
        resume.onSelect = [this]() { closePause(); };
        pause.push_back(std::move(resume));

        MenuItem mapItem;
        mapItem.label = "Karte";
        mapItem.enabled = [this]() { return !arenaMode_; };
        mapItem.onSelect = [this]() { openMap(false); };
        pause.push_back(std::move(mapItem));

        MenuItem settings;
        settings.label = "Einstellungen";
        settings.onSelect = [this]() {
            inSettings_ = true;
            settingsScreen_->open();
        };
        pause.push_back(std::move(settings));

        MenuItem rest;
        rest.label = "Zurück zum letzten Ruheplatz";
        rest.enabled = [this]() { return !arenaMode_; };
        rest.onSelect = [this]() {
            closePause();
            PendingLoad load;
            load.room = state_.restRoom.empty() ? dungeon_.startRoom : state_.restRoom;
            if (!state_.restRoom.empty()) load.position = state_.restPosition;
            else load.door = dungeon_.startDoor;
            load.respawn = false;
            beginTransition(std::move(load));
        };
        pause.push_back(std::move(rest));

        MenuItem quit;
        quit.label = arenaMode_ ? "Arena verlassen" : "Speichern und zum Titel";
        quit.onSelect = [this]() {
            saveGame();
            quitToTitle_ = true;
        };
        pause.push_back(std::move(quit));
    }
    auto uiSound = [this](int k) { ctx_.audio.playUi(k); };
    pauseMenu_.sound = uiSound;
    settingsSound_ = uiSound;
    arenaOverMenu_.sound = uiSound;
    completeMenu_.sound = uiSound;
    {
        std::vector<MenuItem> over;
        MenuItem again;
        again.label = "Nochmal versuchen";
        again.onSelect = [this]() { nextScene_ = std::make_unique<GameScene>(ctx_, bossRush_ ? StartMode::BossRush : StartMode::Arena); };
        over.push_back(std::move(again));
        MenuItem title;
        title.label = "Zum Titelbildschirm";
        title.onSelect = [this]() { quitToTitle_ = true; };
        over.push_back(std::move(title));
        arenaOverMenu_.setItems(std::move(over));
    }
    pauseMenu_.setItems(std::move(pause));
    settingsScreen_ = std::make_unique<SettingsScreen>(ctx_.settings, ctx_.input, [this]() { inSettings_ = false; }, saveSettingsNow);
    settingsScreen_->setSound(settingsSound_);
}

bool GameScene::loadDungeon(const std::string& id) {
    std::string error;
    auto def = DungeonDef::loadFromFile(dataPath("dungeons/" + id + ".json"), error);
    if (!def) {
        Log::error(LogCategory::Loading, "Dungeon '{}' konnte nicht geladen werden: {}", id, error);
        return false;
    }
    dungeon_ = *def;
    return true;
}

void GameScene::rebuildCompleteMenu() {
    std::vector<MenuItem> items;
    if (!dungeon_.nextDungeon.empty()) {
        std::string error;
        auto next = DungeonDef::loadFromFile(dataPath("dungeons/" + dungeon_.nextDungeon + ".json"), error);
        MenuItem go;
        go.label = next ? "Weiter zu: " + next->name : "Nächster Dungeon";
        go.enabled = [ok = next.has_value()]() { return ok; };
        go.onSelect = [this]() { advanceToNextDungeon(); };
        items.push_back(std::move(go));
    }
    MenuItem explore;
    explore.label = "Weiter erkunden";
    explore.onSelect = [this]() {
        phase_ = Phase::Playing;
        setMouseCaptured(true);
        showBanner("Du kannst weiter erkunden", "Esc öffnet das Menü mit \"Speichern und zum Titel\".", 4.0f);
    };
    items.push_back(std::move(explore));
    MenuItem title;
    title.label = "Speichern und zum Titel";
    title.onSelect = [this]() {
        saveGame();
        quitToTitle_ = true;
    };
    items.push_back(std::move(title));
    completeMenu_.setItems(std::move(items));
}

void GameScene::advanceToNextDungeon() {
    if (dungeon_.nextDungeon.empty() || pending_) return;
    DungeonDef previous = dungeon_;
    if (!loadDungeon(dungeon_.nextDungeon)) {
        dungeon_ = previous;
        showToast("Der nächste Dungeon konnte nicht geladen werden, siehe aldoria.log", 6.0f);
        return;
    }
    // Fähigkeiten, Segen, Herzen und Splitter bleiben; Schlüssel, Ruheplatz und Fortschritt gelten pro Dungeon
    state_.dungeonId = dungeon_.id;
    state_.smallKeys = 0;
    state_.bossKey = false;
    state_.restRoom.clear();
    state_.restPosition = {0, 0, 0};
    state_.flaskCharges = state_.flaskMax;
    complete_ = false;
    phase_ = Phase::Playing;
    setMouseCaptured(true);
    player_.healBy(999.0f, ctx_.events);

    PendingLoad load;
    load.room = dungeon_.startRoom;
    load.door = dungeon_.startDoor;
    load.respawn = false;
    beginTransition(std::move(load));
    saveGame();
    Log::info(LogCategory::Save, "Weiter zum Dungeon '{}'", dungeon_.id);
}

void GameScene::syncPlayerFromState() {
    player_.setModifiers(segen_.combine(state_.segen));
    player_.setBonusMaxHealth(10.0f * (float)state_.heartContainers);
    player_.setDashUnlocked(state_.hasAbility("dash"));
    player_.setDoubleJumpUnlocked(state_.hasAbility("double_jump"));
}

const EnemyDef* GameScene::enemyDef(const std::string& type) {
    if (auto it = enemyDefs_.find(type); it != enemyDefs_.end()) return &it->second;
    auto def = EnemyDef::loadFromFile(dataPath("enemies/" + type + ".json"));
    if (!def) return nullptr;
    return &enemyDefs_.emplace(type, std::move(*def)).first->second;
}

const BossDef* GameScene::bossDef(const std::string& type) {
    if (auto it = bossDefs_.find(type); it != bossDefs_.end()) return &it->second;
    auto def = BossDef::loadFromFile(dataPath("enemies/" + type + ".json"));
    if (!def) return nullptr;
    return &bossDefs_.emplace(type, std::move(*def)).first->second;
}

Enemy* GameScene::spawnEnemy(const std::string& type, Vector3 position, float delay) {
    const EnemyDef* def = enemyDef(type);
    if (!def) {
        Log::error(LogCategory::World, "Unbekannter Gegnertyp '{}', übersprungen", type);
        return nullptr;
    }
    enemies_.emplace_back(def, position, delay);
    if (delay > 0.0f) {
        particles_.dustRing({position.x, position.y + 0.05f, position.z}, def->radius, 12, Color{120, 96, 70, 230});
        particles_.burst({position.x, position.y + 0.1f, position.z}, 8, Color{160, 120, 80, 220}, 2.5f, 0.09f, 0.5f);
    }
    return &enemies_.back();
}

// ---------------------------------------------------------------- Raumwechsel

bool GameScene::loadRoom(const std::string& roomId, const std::string& arrivalDoor, std::optional<Vector3> position, std::optional<float> yaw) {
    LevelLoadResult result = loadLevelFromFile(dataPath("rooms/" + roomId + ".json"));
    if (!result.level) {
        Log::error(LogCategory::World, "Raum '{}': {}", roomId, result.error);
        showToast("Raum konnte nicht geladen werden: " + result.error, 6.0f);
        return false;
    }
    Level level = std::move(*result.level);

    Vector3 arrival = level.playerSpawn;
    float arrivalYaw = yaw ? *yaw : level.playerSpawnYaw;
    if (position) {
        arrival = *position;
    } else if (!arrivalDoor.empty()) {
        if (const LevelDoor* d = level.findDoor(arrivalDoor)) {
            arrival = d->arrival;
            arrivalYaw = d->arrivalYaw;
        } else {
            Log::warn(LogCategory::World, "Raum '{}' hat keine Tür '{}', nutze den Startpunkt", roomId, arrivalDoor);
        }
    }

    enemies_.clear();
    projectiles_.clear();
    particles_.clear();
    shockwaves_.clear();
    blasts_.clear();
    floatingTexts_.clear();
    boss_.reset();
    bossFightActive_ = false;
    bossRewardGiven_ = false;
    staticEnemiesKilled_ = 0;
    staticEnemiesTotal_ = 0;

    room_ = std::make_unique<Room>(std::move(level), state_, roomId);
    const Level& lv = room_->level();
    ctx_.renderer.setEnvironment(lv.env);
    ctx_.renderer.setTheme(lv.theme);

    // Feste Gegner des Raums, solange er noch nicht gesäubert ist
    if (!state_.isCleared(roomId)) {
        for (const EnemySpawn& s : lv.enemySpawns) {
            if (Enemy* e = spawnEnemy(s.type, s.position, 0.0f)) {
                e->isStatic = true;
                staticEnemiesTotal_++;
            }
        }
    }

    // Boss: steht bereit, bis der Spieler nahe genug kommt; ist er besiegt, wartet nur noch die Belohnung
    if (lv.boss) {
        if (room_->bossDefeated()) {
            room_->addPersistentPickup("heart", "boss_reward", lv.boss->position);
            bossRewardGiven_ = true;
        } else if (const BossDef* def = bossDef(lv.boss->type)) {
            boss_ = std::make_unique<Boss>(def, lv.boss->position);
        }
    }

    player_.teleport(arrival, arrivalYaw);
    camera_.setYaw(arrivalYaw);
    camera_.snapTo(player_.focusPoint());
    safePosition_ = arrival;
    safeTimer_ = 0.0f;

    state_.currentRoom = roomId;
    bool firstDungeonVisit = !arenaMode_ && roomId == dungeon_.startRoom && state_.visitedRooms.count(roomId) == 0;
    state_.visitedRooms.insert(roomId);
    ctx_.events.emit(RoomEntered{roomId});
    if (firstDungeonVisit) showBanner(dungeon_.name, dungeon_.description, 5.0f);
    else showBanner(lv.name, roomTypeLabel(lv.type), 2.8f);
    Log::info(LogCategory::World, "Raum '{}' geladen ({} Boxen, {} Gegner)", roomId, lv.boxes.size(), enemies_.size());

    if (roomId == dungeon_.finalRoom || lv.type == "end") {
        complete_ = true;
        saveGame();
        rebuildCompleteMenu();
        completeMenu_.select(0);
        bannerTimer_ = 0.0f;
        phase_ = Phase::Complete;
        setMouseCaptured(false);
    }
    return true;
}

void GameScene::beginTransition(PendingLoad load) {
    if (pending_) return;
    pending_ = std::move(load);
    fade_ = Fade::Out;
    fadeTimer_ = 0.0f;
}

void GameScene::updateFade(float rdt) {
    if (fade_ == Fade::None) return;
    fadeTimer_ += rdt;
    if (fade_ == Fade::Out && fadeTimer_ >= kFadeOutTime) {
        if (pending_) {
            PendingLoad load = std::move(*pending_);
            pending_.reset();
            if (load.respawn) {
                state_.deaths++;
                state_.flaskCharges = state_.flaskMax;
                player_.respawn();
                syncPlayerFromState();
                phase_ = Phase::Playing;
                if (!ctx_.input.injectionMode()) setMouseCaptured(true);
            }
            if (!loadRoom(load.room, load.door, load.position, load.yaw)) {
                // Zielraum kaputt: im aktuellen Raum bleiben
                Log::error(LogCategory::World, "Raumwechsel nach '{}' fehlgeschlagen", load.room);
            }
        }
        fade_ = Fade::In;
        fadeTimer_ = 0.0f;
    } else if (fade_ == Fade::In && fadeTimer_ >= kFadeInTime) {
        fade_ = Fade::None;
    }
}

// ---------------------------------------------------------------- Speichern, Ruheplatz, Tod

bool GameScene::saveGame() {
    if (arenaMode_) return true;  // die Arena wird nicht gespeichert
    state_.currentRoom = room_ ? room_->id() : state_.currentRoom;
    std::string error;
    if (!saves_.save("slot1", state_, error)) {
        Log::error(LogCategory::Save, "Speichern fehlgeschlagen: {}", error);
        showToast("Speichern fehlgeschlagen: " + error, 5.0f);
        return false;
    }
    return true;
}

void GameScene::activateCheckpoint() {
    if (!room_ || !room_->level().checkpoint) return;
    Vector3 shrine = *room_->level().checkpoint;
    state_.restRoom = room_->id();
    state_.restPosition = {shrine.x, shrine.y, shrine.z + 2.4f};
    player_.healBy(999.0f, ctx_.events);
    state_.flaskCharges = state_.flaskMax;
    particles_.sparkle({shrine.x, shrine.y + 1.0f, shrine.z}, 30, Color{255, 210, 110, 255}, 1.0f);
    if (saveGame()) showBanner("Ruheplatz", "Leben und Tränke aufgefüllt. Fortschritt gespeichert.", 2.8f);
    ctx_.events.emit(CheckpointActivated{room_->id()});
}

void GameScene::respawnAfterDeath() {
    PendingLoad load;
    load.room = state_.restRoom.empty() ? dungeon_.startRoom : state_.restRoom;
    if (!state_.restRoom.empty()) load.position = state_.restPosition;
    else load.door = dungeon_.startDoor;
    load.respawn = true;
    beginTransition(std::move(load));
}

// ---------------------------------------------------------------- Update

void GameScene::setMouseCaptured(bool captured) {
    mouseCaptured_ = captured;
    skipLook_ = true;
    if (ctx_.input.injectionMode()) return;
    if (captured) DisableCursor();
    else EnableCursor();
}

void GameScene::openPause() {
    phase_ = Phase::Paused;
    inSettings_ = false;
    pauseMenu_.select(0);
    setMouseCaptured(false);
}

void GameScene::openMap(bool fromPlaying) {
    if (arenaMode_) return;
    if (map_.empty() || mapDungeonId_ != dungeon_.id) {
        map_ = loadDungeonMap(dungeon_.startRoom);
        mapDungeonId_ = dungeon_.id;
    }
    phase_ = Phase::Paused;
    inSettings_ = false;
    showMap_ = true;
    mapFromPlaying_ = fromPlaying;
    setMouseCaptured(false);
}

void GameScene::closePause() {
    showMap_ = false;
    phase_ = Phase::Playing;
    inSettings_ = false;
    setMouseCaptured(true);
}

void GameScene::update(float dt) {
    float rdt = ctx_.input.injectionMode() ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.05f);
    time_ += rdt;
    toastTimer_ = std::max(0.0f, toastTimer_ - rdt);
    bannerTimer_ = std::max(0.0f, bannerTimer_ - rdt);

    if (IsKeyPressed(KEY_F1)) debug_.visible = !debug_.visible;
    if (IsKeyPressed(KEY_F3)) debug_.showCollision = !debug_.showCollision;
    camera_.setShakeScale(ctx_.settings.cameraShake);

    updateFade(rdt);
    ctx_.audio.setMusic(phase_ == Phase::Playing ? currentMood() : MusicKind::Explore);

    switch (phase_) {
        case Phase::Playing:
            if (fade_ != Fade::Out) {
                if (IsKeyPressed(KEY_F5) && room_) loadRoom(room_->id(), "", player_.position(), player_.yaw());
                updatePlaying(dt);
            }
            break;
        case Phase::Paused: updatePaused(); break;
        case Phase::Segen: updateSegen(); break;
        case Phase::Dying: updateDying(dt); break;
        case Phase::ArenaOver:
            arenaOverMenu_.update(ctx_.input);
            updateEffectsAndTexts(dt);
            break;
        case Phase::Complete:
            completeMenu_.update(ctx_.input);
            updateEffectsAndTexts(dt);
            break;
    }
}

void GameScene::updatePaused() {
    Input& in = ctx_.input;
    if (showMap_) {
        if (in.pressed(Action::UiBack) || in.pressed(Action::Map) || in.pressed(Action::UiConfirm)) {
            showMap_ = false;
            if (mapFromPlaying_) closePause();
        }
        return;
    }
    if (inSettings_) {
        settingsScreen_->update(in);
    } else {
        pauseMenu_.update(in);
        if (in.pressed(Action::UiBack)) closePause();
    }
}

bool GameScene::segenAvailable() const {
    for (const SegenDef& s : segen_.all) {
        if (std::find(state_.segen.begin(), state_.segen.end(), s.id) == state_.segen.end()) return true;
    }
    return false;
}

void GameScene::openSegenChoice(int shardCost) {
    std::vector<const SegenDef*> pool;
    for (const SegenDef& s : segen_.all) {
        if (std::find(state_.segen.begin(), state_.segen.end(), s.id) == state_.segen.end()) pool.push_back(&s);
    }
    if (pool.empty()) return;
    segenChoices_.clear();
    while (segenChoices_.size() < 3 && !pool.empty()) {
        size_t i = (size_t)GetRandomValue(0, (int)pool.size() - 1);
        segenChoices_.push_back(pool[i]);
        pool.erase(pool.begin() + (long)i);
    }
    segenSelected_ = 0;
    segenCost_ = shardCost;
    phase_ = Phase::Segen;
    setMouseCaptured(false);
}

void GameScene::updateSegen() {
    Input& in = ctx_.input;
    int n = (int)segenChoices_.size();
    if (n == 0) {
        phase_ = Phase::Playing;
        setMouseCaptured(true);
        return;
    }
    if (segenCost_ > 0 && in.pressed(Action::UiBack)) {
        segenChoices_.clear();
        segenCost_ = 0;
        phase_ = Phase::Playing;
        setMouseCaptured(true);
        return;
    }
    if (in.pressed(Action::UiLeft)) segenSelected_ = (segenSelected_ + n - 1) % n;
    if (in.pressed(Action::UiRight)) segenSelected_ = (segenSelected_ + 1) % n;
    if (in.pressed(Action::UiConfirm)) {
        const SegenDef* chosen = segenChoices_[(size_t)segenSelected_];
        if (segenCost_ > 0) state_.shards = std::max(0, state_.shards - segenCost_);
        segenCost_ = 0;
        state_.segen.push_back(chosen->id);
        syncPlayerFromState();
        ctx_.events.emit(SegenChosen{chosen->id});
        Vector3 p = player_.position();
        particles_.sparkle({p.x, p.y + 1.0f, p.z}, 30, Color{255, 220, 130, 255}, 0.8f);
        showToast("Segen erhalten: " + chosen->name);
        segenChoices_.clear();
        phase_ = Phase::Playing;
        setMouseCaptured(true);
    }
}

MusicKind GameScene::currentMood() const {
    if (!room_) return MusicKind::Explore;
    if (bossFightActive_ && boss_ && boss_->alive()) return MusicKind::Boss;
    if (room_->inCombat()) return MusicKind::Combat;
    for (const Enemy& e : enemies_) {
        bool active = e.state() == EnemyState::Chase || e.state() == EnemyState::Telegraph || e.state() == EnemyState::Attack;
        if (active && Vector3Distance(e.position(), player_.position()) < 14.0f) return MusicKind::Combat;
    }
    return MusicKind::Explore;
}

int GameScene::aliveEnemyCount() const {
    int n = 0;
    for (const Enemy& e : enemies_) {
        if (e.alive()) n++;
    }
    return n;
}

std::optional<Vector3> GameScene::findAssistTarget() const {
    const Enemy* best = nullptr;
    float bestDist = kAssistRange;
    for (const Enemy& e : enemies_) {
        if (!e.targetable()) continue;
        float d = Vector3Distance(e.position(), player_.position());
        if (d < bestDist) {
            bestDist = d;
            best = &e;
        }
    }
    if (best) return best->position();
    if (boss_ && boss_->targetable() && Vector3Distance(boss_->position(), player_.position()) < kAssistRange + boss_->radius()) {
        return boss_->position();
    }
    return std::nullopt;
}

void GameScene::updatePlaying(float dt) {
    Input& in = ctx_.input;
    if (in.pressed(Action::Pause)) {
        openPause();
        return;
    }
    if (in.pressed(Action::Map) && !arenaMode_) {
        openMap(true);
        return;
    }

    // Kamera-Blick. Beim Fensterwechsel springt die Maus oft; diesen Ruck nicht in die Kamera geben.
    bool focused = IsWindowFocused() || in.injectionMode();
    if (focused != wasFocused_) skipLook_ = true;
    wasFocused_ = focused;
    float rdt = in.injectionMode() ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.05f);
    Vector2 look = in.lookDelta(rdt, mouseCaptured_ && focused && !skipLook_);
    skipLook_ = false;
    constexpr float kMaxLookPerFrame = 0.52f;  // nie mehr als etwa 30 Grad pro Frame
    look.x = std::clamp(look.x, -kMaxLookPerFrame, kMaxLookPerFrame);
    look.y = std::clamp(look.y, -kMaxLookPerFrame, kMaxLookPerFrame);

    PlayerFrame frame;
    frame.level = &room_->level();
    frame.cameraYaw = camera_.yaw();
    frame.assistTarget = findAssistTarget();
    player_.godMode = godMode_;
    player_.update(dt, makePlayerInput(in, true), frame, ctx_.events);

    if (dt > 0.0f && !player_.dead()) {
        if (in.pressed(Action::Heal)) tryDrinkFlask();

        Vector3 origin, direction;
        if (player_.consumeCast(origin, direction)) spawnSpark(origin, direction);

        // Ruheplatz
        if (room_->level().checkpoint && in.pressed(Action::Interact)) {
            Vector3 s = *room_->level().checkpoint;
            if (Vector3Distance({s.x, player_.position().y, s.z}, player_.position()) < 2.8f) activateCheckpoint();
        }
        // Am Ruheplatz Splitter gegen einen Segen tauschen
        if (room_->level().checkpoint && !arenaMode_ && in.pressed(Action::Ability) && segenAvailable()) {
            Vector3 s = *room_->level().checkpoint;
            if (Vector3Distance({s.x, player_.position().y, s.z}, player_.position()) < 2.8f) {
                int cost = shrineSegenCost();
                if (state_.shards >= cost) openSegenChoice(cost);
                else showToast(std::format("Für einen Segen brauchst du {} Splitter", cost));
            }
        }
    }

    if (dt > 0.0f) {
        updateEnemies(dt);
        updateBoss(dt);
        projectiles_.update(dt, room_->level());
        resolveProjectileHits();
        updateShockwaves(dt);
        updateBlasts(dt);

        // Raum: Plattformen, Schalter, Türen, Pickups, Arena
        std::vector<RoomActor> actors;
        actors.push_back({&player_.body(), true, !player_.dead()});
        for (Enemy& e : enemies_) actors.push_back({&e.body(), false, e.alive()});
        room_->update(dt, actors, aliveEnemyCount(), ctx_.events);

        for (const SpawnRequest& s : room_->takeSpawnRequests()) spawnEnemy(s.type, s.position, 0.9f);
        if (auto t = room_->takeTransition()) {
            PendingLoad load;
            load.room = t->room;
            load.door = t->door;
            beginTransition(std::move(load));
        }
        if (room_->takeEncounterCleared() && room_->level().encounter && room_->level().encounter->reward == "segen") {
            openSegenChoice();
        }

        if (arenaMode_) updateArena(dt);
        resolveCombat();
        processBossEvents();
        updateHazardsAndFalls(dt);
        separateEnemies();
        std::erase_if(enemies_, [](const Enemy& e) { return e.removable(); });

        if (boss_ && !bossFightActive_ && boss_->alive() && !player_.dead() &&
            Vector3Distance(boss_->position(), player_.position()) < kBossTriggerDistance) {
            startBossFight();
        }
    }

    camera_.updateFollow(dt, look, player_.focusPoint(), room_->level());
    updateEffectsAndTexts(dt);
    state_.playTime += rdt;

    if (player_.dead() && phase_ == Phase::Playing) {
        phase_ = Phase::Dying;
        deathTimer_ = 0.0f;
    }
}

void GameScene::updateDying(float dt) {
    float rdt = ctx_.input.injectionMode() ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.05f);
    deathTimer_ += rdt;
    // Die Welt läuft weiter, damit der Tod nicht wie ein Standbild wirkt
    player_.update(dt, PlayerInput{}, PlayerFrame{&room_->level(), camera_.yaw(), std::nullopt}, ctx_.events);
    updateEffectsAndTexts(dt);
    camera_.updateFollow(dt, {0, 0}, player_.focusPoint(), room_->level());
    if (deathTimer_ >= kRespawnDelay && fade_ == Fade::None) {
        if (arenaMode_) {
            // In der Arena gibt es kein Wiederbeleben: Ergebnis zeigen
            if (director_ && director_->wave() > arenaBest_) {
                arenaBest_ = director_->wave();
                saveArenaBest();
            }
            phase_ = Phase::ArenaOver;
            arenaOverMenu_.select(0);
            setMouseCaptured(false);
        } else {
            respawnAfterDeath();
        }
    }
}

// Funken, die aus der Lava aufsteigen (nur in Spielernähe, sonst sind es zu viele)
void GameScene::spawnLavaEmbers(float dt) {
    if (!room_ || dt <= 0.0f) return;
    const Level& lv = room_->level();
    Vector3 pp = player_.position();
    for (const LevelHazard& h : lv.hazards) {
        if (h.kind != HazardKind::Lava) continue;
        const LevelBox& b = lv.boxes[(size_t)h.boxIndex];
        float x0 = std::max(b.minX(), pp.x - 12.0f), x1 = std::min(b.maxX(), pp.x + 12.0f);
        float z0 = std::max(b.minZ(), pp.z - 12.0f), z1 = std::min(b.maxZ(), pp.z + 12.0f);
        if (x1 <= x0 || z1 <= z0) continue;
        float expected = std::min((x1 - x0) * (z1 - z0) * 0.08f, 9.0f) * dt;
        int n = (int)expected;
        if ((float)GetRandomValue(0, 1000) / 1000.0f < expected - (float)n) n++;
        for (int i = 0; i < n; i++) {
            float x = x0 + (float)GetRandomValue(0, 1000) / 1000.0f * (x1 - x0);
            float z = z0 + (float)GetRandomValue(0, 1000) / 1000.0f * (z1 - z0);
            particles_.ember({x, b.maxY() + 0.05f, z}, Color{255, 156, 56, 240});
        }
    }
}

void GameScene::updateEffectsAndTexts(float dt) {
    particles_.update(dt);
    hurtFlash_ = std::max(0.0f, hurtFlash_ - dt * 1.6f);
    spawnLavaEmbers(dt);
    for (FloatingText& t : floatingTexts_) {
        t.age += dt;
        t.position.y += 1.3f * dt;
    }
    std::erase_if(floatingTexts_, [](const FloatingText& t) { return t.age >= kFloatingTextLife; });
    for (const Vector3& p : projectiles_.takeImpacts()) {
        particles_.burst(p, 8, Color{255, 190, 90, 230}, 3.5f, 0.07f, 0.35f);
    }
}

// ---------------------------------------------------------------- Gegner, Boss, Kampf

void GameScene::updateEnemies(float dt) {
    for (Enemy& e : enemies_) {
        e.update(dt, room_->level(), player_.position(), !player_.dead());
        Vector3 origin, direction;
        if (e.consumeShot(origin, direction)) {
            ctx_.events.emit(EnemyShot{});
            Projectile p;
            p.position = origin;
            p.velocity = Vector3Scale(direction, e.def().projectileSpeed);
            p.damage = e.def().damage;
            p.radius = 0.3f;
            p.life = 4.0f;
            p.knockback = 5.0f;
            p.fromPlayer = false;
            p.color = Color{130, 170, 255, 255};
            projectiles_.spawn(p);
        }
    }
}

void GameScene::separateEnemies() {
    const float playerRadius = player_.config().radius;
    const Level& level = room_->level();
    for (size_t i = 0; i < enemies_.size(); i++) {
        Enemy& a = enemies_[i];
        if (!a.alive() || a.state() == EnemyState::Spawning) continue;

        // Gegner laufen nicht in den Spieler hinein
        {
            Vector3 d{a.position().x - player_.position().x, 0.0f, a.position().z - player_.position().z};
            float dist = Vector3Length(d);
            float minDist = a.radius() + playerRadius;
            if (dist < minDist && dist > 1e-4f && std::fabs(a.position().y - player_.position().y) < 1.5f) {
                float push = minDist - dist;
                a.body().position.x += d.x / dist * push;
                a.body().position.z += d.z / dist * push;
                level.resolveHorizontal(a.body().position, a.radius(), a.height());
            }
        }

        // Gegner schieben sich gegenseitig auseinander
        for (size_t j = i + 1; j < enemies_.size(); j++) {
            Enemy& b = enemies_[j];
            if (!b.alive() || b.state() == EnemyState::Spawning) continue;
            Vector3 d{b.position().x - a.position().x, 0.0f, b.position().z - a.position().z};
            float dist = Vector3Length(d);
            float minDist = a.radius() + b.radius();
            if (dist >= minDist) continue;
            if (dist < 1e-4f) {
                d = {1.0f, 0.0f, 0.0f};
                dist = 1.0f;
            }
            float push = (minDist - dist) * 0.5f;
            a.body().position.x -= d.x / dist * push;
            a.body().position.z -= d.z / dist * push;
            b.body().position.x += d.x / dist * push;
            b.body().position.z += d.z / dist * push;
            level.resolveHorizontal(a.body().position, a.radius(), a.height());
            level.resolveHorizontal(b.body().position, b.radius(), b.height());
        }
    }
}

void GameScene::updateBoss(float dt) {
    if (!boss_) return;
    boss_->update(dt, room_->level(), player_.position(), !player_.dead());

    // Ansturm: Berührung während des Rennens tut weh (mit Strg durchrollen oder zur Seite gehen)
    if (boss_->chargeHitActive() && !player_.dead()) {
        Vector3 d{boss_->position().x - player_.position().x, 0.0f, boss_->position().z - player_.position().z};
        float reach = boss_->radius() + player_.config().radius + 0.3f;
        if (Vector3Length(d) < reach && std::fabs(boss_->position().y - player_.position().y) < 2.5f) {
            Vector3 dir = boss_->chargeDirection();
            if (damagePlayer(boss_->def().chargeDamage, dir, boss_->def().chargeKnockback) > 0.0f) boss_->consumeCharge();
        }
    }

    // Der Spieler wird nicht in den Boss hineingeschoben
    if (boss_->alive() && boss_->started()) {
        Vector3 d{boss_->position().x - player_.position().x, 0.0f, boss_->position().z - player_.position().z};
        float dist = Vector3Length(d);
        float minDist = boss_->radius() + player_.config().radius;
        if (dist < minDist && dist > 1e-4f && std::fabs(boss_->position().y - player_.position().y) < 2.5f) {
            float push = minDist - dist;
            player_.body().position.x -= d.x / dist * push;
            player_.body().position.z -= d.z / dist * push;
            room_->level().resolveHorizontal(player_.body().position, player_.config().radius, player_.config().height);
        }
    }

    if (!boss_->alive() && !bossRewardGiven_) {
        bossRewardGiven_ = true;
        bossFightActive_ = false;
        Vector3 p = boss_->position();
        room_->setBossDefeated(ctx_.events);
        if (arenaMode_) player_.healBy(player_.health().max() * 0.5f, ctx_.events);
        else room_->addPersistentPickup("heart", "boss_reward", {p.x, p.y + 0.5f, p.z});
        particles_.burst({p.x, p.y + 1.5f, p.z}, 60, Color{255, 200, 90, 255}, 9.0f, 0.22f, 1.4f, 4.0f);
        particles_.burst({p.x, p.y + 1.5f, p.z}, 40, Color{255, 90, 60, 255}, 6.0f, 0.18f, 1.2f, 4.0f);
        camera_.addShake(0.35f);
        showBanner(std::format("{} besiegt!", boss_->def().name), arenaMode_ ? "" : "Nimm das Herz und geh durch das Tor.", 4.0f);
        // Beschworene Diener fallen mit ihm
        for (Enemy& e : enemies_) {
            if (e.alive()) e.takeHit(99999.0f, {0, 0, 0});
        }
    }
}

void GameScene::startBossFight() {
    if (!boss_) return;
    bossFightActive_ = true;
    boss_->start();
    room_->setBossFight(true);
    showBanner(boss_->def().name, boss_->def().introText, 3.2f);
    ctx_.events.emit(BossRoar{});
    camera_.addShake(0.12f);
}

float GameScene::damagePlayer(float amount, Vector3 knockbackDirection, float knockback) {
    float dealt = player_.takeDamage(amount, Vector3Scale(knockbackDirection, knockback), ctx_.events);
    if (dealt <= 0.0f) return 0.0f;
    ctx_.time.hitstop(0.07f);
    camera_.addShake(0.09f);
    hurtFlash_ = 1.0f;
    Vector3 head{player_.position().x, player_.position().y + player_.config().height + 0.2f, player_.position().z};
    addFloatingText(head, "-" + std::to_string((int)std::round(dealt)), Color{255, 110, 100, 255});
    particles_.burst({player_.position().x, player_.position().y + 1.0f, player_.position().z}, 12, Color{255, 110, 100, 230}, 4.0f, 0.09f, 0.45f);
    return dealt;
}

float GameScene::dealPlayerDamageRoll(float base, bool& crit) {
    const PlayerModifiers& m = player_.modifiers();
    float dmg = base * m.damageMult;
    crit = randomUnit() < m.critChance;
    if (crit) dmg *= 2.0f;
    return dmg;
}

void GameScene::onEnemyKilled(Enemy& e) {
    if (director_) director_->addKill();
    Vector3 p = e.position();
    Log::info(LogCategory::Combat, "{} besiegt", e.def().name);
    ctx_.events.emit(EnemyDied{e.def().id});
    particles_.burst({p.x, p.y + e.height() * 0.5f, p.z}, 18, e.def().color, 5.0f, 0.12f, 0.7f);
    if (player_.modifiers().healOnKill > 0.0f) player_.healBy(player_.modifiers().healOnKill, ctx_.events);
    if (randomUnit() < e.def().healthDropChance) room_->addPickup("health_drop", {p.x, p.y + 0.5f, p.z});

    if (e.isStatic) {
        staticEnemiesKilled_++;
        if (staticEnemiesKilled_ >= staticEnemiesTotal_ && !room_->level().encounter && !room_->level().boss) {
            room_->markCleared();
        }
    }
}

void GameScene::resolveCombat() {
    // Spieler trifft Gegner, Boss und Kristalle
    if (player_.attackActive()) {
        const AttackDef& attack = player_.currentAttack();
        MeleeQuery query = player_.attackQuery();
        const PlayerModifiers& mods = player_.modifiers();
        room_->hitCrystals(query, ctx_.events);

        for (Enemy& e : enemies_) {
            if (!e.targetable() || e.lastHitSwing == player_.swingId()) continue;
            if (!meleeHits(query, e.position(), e.radius(), e.height())) continue;
            e.lastHitSwing = player_.swingId();

            bool crit = false;
            float dmg = dealPlayerDamageRoll(attack.damage, crit);
            Vector3 away = horizontalDirection(player_.position(), e.position(), player_.yaw());
            float dealt = e.takeHit(dmg, Vector3Scale(away, attack.knockback * mods.knockbackMult));
            ctx_.time.hitstop(attack.hitstop);
            camera_.addShake(0.015f + attack.hitstop * 0.5f);

            Vector3 head{e.position().x, e.position().y + e.height() + 0.3f, e.position().z};
            addFloatingText(head, std::to_string((int)std::round(dealt)) + (crit ? "!" : ""), crit ? Color{255, 210, 80, 255} : Color{255, 244, 214, 255});
            particles_.burst({e.position().x, e.position().y + e.height() * 0.6f, e.position().z}, crit ? 12 : 7, Color{255, 236, 190, 255}, 4.5f, 0.07f, 0.3f);
            ctx_.events.emit(EnemyDamaged{e.def().id, dealt});
            if (!e.alive()) onEnemyKilled(e);
        }

        if (boss_ && boss_->targetable() && boss_->lastHitSwing != player_.swingId() &&
            meleeHits(query, boss_->position(), boss_->radius(), boss_->height())) {
            boss_->lastHitSwing = player_.swingId();
            bool crit = false;
            float dmg = dealPlayerDamageRoll(attack.damage, crit);
            float dealt = boss_->takeHit(dmg, {0, 0, 0});
            ctx_.time.hitstop(attack.hitstop);
            camera_.addShake(0.03f);
            Vector3 head{boss_->position().x, boss_->position().y + boss_->height() + 0.3f, boss_->position().z};
            addFloatingText(head, std::to_string((int)std::round(dealt)) + (boss_->exhausted() ? "!!" : ""), boss_->exhausted() ? Color{160, 200, 255, 255} : Color{255, 244, 214, 255});
            particles_.burst({boss_->position().x, boss_->position().y + boss_->height() * 0.6f, boss_->position().z}, 10, Color{255, 236, 190, 255}, 5.0f, 0.09f, 0.35f);
            ctx_.events.emit(EnemyDamaged{boss_->def().id, dealt});
        }
    }

    // Gegner treffen Spieler
    for (Enemy& e : enemies_) {
        if (!e.attackHitActive()) continue;
        if (!meleeHits(e.attackQuery(), player_.position(), player_.config().radius, player_.config().height)) continue;
        if (!player_.canBeHit()) continue;  // ausgewichen oder gerade erst getroffen: der Angriff bleibt aktiv
        Vector3 dir = horizontalDirection(e.position(), player_.position(), e.yaw());
        if (damagePlayer(e.def().damage, dir, e.def().knockback) > 0.0f) e.consumeAttack();
    }

    // Boss-Hieb
    if (boss_ && boss_->swingHitActive() && meleeHits(boss_->swingQuery(), player_.position(), player_.config().radius, player_.config().height) &&
        player_.canBeHit()) {
        Vector3 dir = horizontalDirection(boss_->position(), player_.position(), boss_->yaw());
        if (damagePlayer(boss_->def().swingDamage, dir, boss_->def().swingKnockback) > 0.0f) boss_->consumeSwing();
    }
}

void GameScene::spawnSpark(Vector3 origin, Vector3 direction) {
    Projectile p;
    const PlayerModifiers& m = player_.modifiers();
    p.position = origin;
    p.velocity = Vector3Scale(direction, player_.config().sparkSpeed);
    p.damage = player_.config().sparkDamage * m.sparkDamageMult * m.damageMult;
    p.radius = 0.26f;
    p.life = 1.6f;
    p.knockback = 4.0f;
    p.fromPlayer = true;
    p.color = Color{255, 176, 70, 255};
    projectiles_.spawn(p);
    particles_.burst(origin, 6, Color{255, 200, 100, 255}, 3.0f, 0.07f, 0.25f, 0.0f);
}

void GameScene::resolveProjectileHits() {
    for (Projectile& p : projectiles_.all()) {
        if (p.dead) continue;

        if (p.fromPlayer) {
            room_->hitCrystalsAt(p.position, p.radius, ctx_.events);
            for (Enemy& e : enemies_) {
                if (!e.targetable()) continue;
                float dx = e.position().x - p.position.x, dz = e.position().z - p.position.z;
                if (std::sqrt(dx * dx + dz * dz) > e.radius() + p.radius) continue;
                if (p.position.y < e.position().y - 0.2f || p.position.y > e.position().y + e.height() + 0.2f) continue;
                Vector3 dir = Vector3Normalize(Vector3{p.velocity.x, 0.0f, p.velocity.z});
                float dealt = e.takeHit(p.damage, Vector3Scale(dir, p.knockback));
                p.dead = true;
                ctx_.time.hitstop(0.04f);
                Vector3 head{e.position().x, e.position().y + e.height() + 0.3f, e.position().z};
                addFloatingText(head, std::to_string((int)std::round(dealt)), Color{255, 190, 90, 255});
                particles_.burst(p.position, 10, Color{255, 170, 60, 255}, 5.0f, 0.08f, 0.4f);
                ctx_.events.emit(EnemyDamaged{e.def().id, dealt});
                if (!e.alive()) onEnemyKilled(e);
                break;
            }
            if (!p.dead && boss_ && boss_->targetable()) {
                float dx = boss_->position().x - p.position.x, dz = boss_->position().z - p.position.z;
                if (std::sqrt(dx * dx + dz * dz) < boss_->radius() + p.radius && p.position.y > boss_->position().y - 0.2f &&
                    p.position.y < boss_->position().y + boss_->height() + 0.2f) {
                    float dealt = boss_->takeHit(p.damage * boss_->def().sparkResist, {0, 0, 0});
                    p.dead = true;
                    Vector3 head{boss_->position().x, boss_->position().y + boss_->height() + 0.3f, boss_->position().z};
                    addFloatingText(head, std::to_string((int)std::round(dealt)), Color{255, 190, 90, 255});
                    particles_.burst(p.position, 10, Color{255, 170, 60, 255}, 5.0f, 0.08f, 0.4f);
                    ctx_.events.emit(EnemyDamaged{boss_->def().id, dealt});
                }
            }
        } else if (!player_.dead() && player_.canBeHit()) {
            const Vector3 pp = player_.position();
            float dx = pp.x - p.position.x, dz = pp.z - p.position.z;
            if (std::sqrt(dx * dx + dz * dz) < player_.config().radius + p.radius && p.position.y > pp.y - 0.1f &&
                p.position.y < pp.y + player_.config().height + 0.1f) {
                Vector3 dir = Vector3Normalize(Vector3{p.velocity.x, 0.0f, p.velocity.z});
                if (damagePlayer(p.damage, dir, p.knockback) > 0.0f) p.dead = true;
            }
        }
    }
}

void GameScene::processBossEvents() {
    if (!boss_) return;
    for (const BossEvent& e : boss_->takeEvents()) {
        const Vector3 pp = player_.position();
        switch (e.type) {
            case BossEvent::Type::Stomp: {
                ctx_.events.emit(BossStomp{});
                particles_.dustRing({e.position.x, e.position.y + 0.05f, e.position.z}, e.radius, 48, Color{200, 180, 150, 220});
                camera_.addShake(0.22f);
                ctx_.time.hitstop(0.05f);
                float dist = std::sqrt((pp.x - e.position.x) * (pp.x - e.position.x) + (pp.z - e.position.z) * (pp.z - e.position.z));
                // Man kann über das Stampfen hinwegspringen
                if (dist < e.radius + player_.config().radius && pp.y < e.position.y + 0.7f) {
                    damagePlayer(e.damage, horizontalDirection(e.position, pp, 0.0f), 9.0f);
                }
                break;
            }
            case BossEvent::Type::Shockwave: {
                ctx_.events.emit(BossStomp{});
                Shockwave w;
                w.center = e.position;
                w.speed = e.speed;
                w.maxRadius = e.radius;
                w.damage = e.damage;
                shockwaves_.push_back(w);
                camera_.addShake(0.18f);
                particles_.dustRing({e.position.x, e.position.y + 0.05f, e.position.z}, 3.0f, 30, Color{230, 200, 150, 220});
                break;
            }
            case BossEvent::Type::Volley: {
                ctx_.events.emit(BossVolleyFired{});
                int n = std::max(1, e.count);
                // Die Feuerbälle fliegen auf die Brusthöhe des Spielers: am Boden treffen sie, ausweichen geht mit Strg oder Sprung
                Vector3 toTarget = Vector3Subtract(e.target, e.position);
                float flat = std::max(0.5f, std::hypot(toTarget.x, toTarget.z));
                float slope = std::clamp(toTarget.y / flat, -0.7f, 0.4f);
                float baseYaw = std::atan2(toTarget.x, toTarget.z);
                for (int i = 0; i < n; i++) {
                    float t = n == 1 ? 0.0f : (float)i / (float)(n - 1) - 0.5f;
                    float a = baseYaw + t * e.radius * 2.0f;
                    Projectile p;
                    p.position = e.position;
                    p.velocity = {std::sin(a) * e.speed, slope * e.speed, std::cos(a) * e.speed};
                    p.damage = e.damage;
                    p.radius = 0.34f;
                    p.life = 4.0f;
                    p.knockback = 6.0f;
                    p.color = Color{255, 110, 50, 255};
                    projectiles_.spawn(p);
                }
                particles_.burst(e.position, 12, Color{255, 150, 70, 255}, 4.0f, 0.1f, 0.4f, 0.0f);
                break;
            }
            case BossEvent::Type::SpiralShot: {
                ctx_.events.emit(BossVolleyFired{});
                Projectile p;
                p.position = {e.position.x, e.position.y + 0.95f, e.position.z};
                p.velocity = {e.direction.x * e.speed, 0.0f, e.direction.z * e.speed};
                p.damage = e.damage;
                p.radius = 0.3f;
                p.life = 3.4f;
                p.knockback = 5.0f;
                p.color = boss_->def().style == "storm" ? Color{110, 200, 255, 255} : Color{255, 110, 50, 255};
                projectiles_.spawn(p);
                break;
            }
            case BossEvent::Type::Meteors: {
                ctx_.events.emit(BossRoar{});
                const Level& lv = room_->level();
                float hx = lv.shellSize.x > 0 ? lv.shellSize.x * 0.5f - 1.5f : 20.0f;
                float hz = lv.shellSize.y > 0 ? lv.shellSize.y * 0.5f - 1.5f : 20.0f;
                for (int i = 0; i < e.count; i++) {
                    Blast b;
                    // Der erste Einschlag zielt auf den Spieler, die übrigen verteilen sich um ihn
                    Vector3 c = pp;
                    if (i > 0) {
                        float ang = randomUnit() * 6.2831853f;
                        float rad = 2.0f + randomUnit() * 6.0f;
                        c = {pp.x + std::cos(ang) * rad, pp.y, pp.z + std::sin(ang) * rad};
                    }
                    c.x = std::clamp(c.x, -hx, hx);
                    c.z = std::clamp(c.z, -hz, hz);
                    b.center = {c.x, lv.groundY, c.z};
                    b.radius = e.radius;
                    b.total = b.delay = e.speed + 0.12f * (float)i;
                    b.damage = e.damage;
                    b.color = boss_->def().style == "storm" ? Color{130, 200, 255, 255} : Color{255, 90, 50, 255};
                    blasts_.push_back(b);
                }
                camera_.addShake(0.1f);
                break;
            }
            case BossEvent::Type::ChargeCrash: {
                ctx_.events.emit(BossStomp{});
                particles_.dustRing({e.position.x, e.position.y + 0.05f, e.position.z}, e.radius, 30, Color{200, 180, 150, 220});
                particles_.burst({e.position.x, e.position.y + 1.2f, e.position.z}, 24, Color{255, 230, 150, 255}, 6.0f, 0.12f, 0.6f);
                camera_.addShake(0.3f);
                ctx_.time.hitstop(0.07f);
                showToast("Der Boss ist benommen!", 1.5f);
                break;
            }
            case BossEvent::Type::Summon: {
                ctx_.events.emit(BossRoar{});
                int alive = aliveEnemyCount();
                int count = std::min(e.count, std::max(0, 5 - alive));
                for (int i = 0; i < count; i++) {
                    float a = 6.2831853f * ((float)i / (float)std::max(1, count)) + 0.6f;
                    Vector3 pos{e.position.x + std::cos(a) * 4.2f, e.position.y, e.position.z + std::sin(a) * 4.2f};
                    room_->level().resolveHorizontal(pos, 0.45f, 1.1f);
                    spawnEnemy(boss_->def().summonType, pos, 1.1f);
                }
                camera_.addShake(0.1f);
                break;
            }
            case BossEvent::Type::LeapLand: {
                ctx_.events.emit(BossStomp{});
                particles_.dustRing({e.position.x, e.position.y + 0.05f, e.position.z}, e.radius, 40, Color{210, 190, 160, 220});
                camera_.addShake(0.3f);
                ctx_.time.hitstop(0.06f);
                float dist = std::sqrt((pp.x - e.position.x) * (pp.x - e.position.x) + (pp.z - e.position.z) * (pp.z - e.position.z));
                if (dist < e.radius + player_.config().radius && pp.y < e.position.y + 1.2f) {
                    damagePlayer(e.damage, horizontalDirection(e.position, pp, 0.0f), 10.0f);
                }
                break;
            }
            case BossEvent::Type::PhaseChange:
                camera_.addShake(0.25f);
                particles_.burst({e.position.x, e.position.y + 1.5f, e.position.z}, 40, Color{255, 230, 150, 255}, 7.0f, 0.16f, 0.9f, 2.0f);
                ctx_.events.emit(BossPhaseChanged{boss_->def().id, e.phase});
                break;
        }
    }
}

void GameScene::updateBlasts(float dt) {
    if (blasts_.empty()) return;
    for (Blast& b : blasts_) b.delay -= dt;
    const Vector3 pp = player_.position();
    for (const Blast& b : blasts_) {
        if (b.delay > 0.0f) continue;
        ctx_.events.emit(BossStomp{});
        particles_.dustRing({b.center.x, b.center.y + 0.05f, b.center.z}, b.radius, 24, Color{b.color.r, b.color.g, b.color.b, 200});
        particles_.burst({b.center.x, b.center.y + 0.6f, b.center.z}, 22, Color{255, 200, 90, 255}, 6.5f, 0.13f, 0.6f, 4.0f);
        camera_.addShake(0.12f);
        float dx = pp.x - b.center.x, dz = pp.z - b.center.z;
        if (std::sqrt(dx * dx + dz * dz) < b.radius + player_.config().radius && pp.y < b.center.y + 2.0f && !player_.dead()) {
            damagePlayer(b.damage, horizontalDirection(b.center, pp, 0.0f), 8.0f);
        }
    }
    std::erase_if(blasts_, [](const Blast& b) { return b.delay <= 0.0f; });
}

void GameScene::updateShockwaves(float dt) {
    const float playerRadius = player_.config().radius;
    for (Shockwave& w : shockwaves_) {
        w.radius += w.speed * dt;
        if (w.hit || player_.dead()) continue;
        Vector3 pp = player_.position();
        float dist = std::sqrt((pp.x - w.center.x) * (pp.x - w.center.x) + (pp.z - w.center.z) * (pp.z - w.center.z));
        bool onRing = std::fabs(dist - w.radius) < 0.8f + playerRadius;
        // Wer beim Auftreffen springt oder gerade in der Luft ist, wird nicht getroffen
        if (onRing && pp.y < w.center.y + 0.5f && player_.canBeHit()) {
            if (damagePlayer(w.damage, horizontalDirection(w.center, pp, 0.0f), 8.0f) > 0.0f) w.hit = true;
        }
    }
    std::erase_if(shockwaves_, [](const Shockwave& w) { return w.radius > w.maxRadius; });
}

// ---------------------------------------------------------------- Gefahren, Abstürze, Tränke

void GameScene::updateHazardsAndFalls(float dt) {
    if (player_.dead()) return;
    const Level& level = room_->level();
    const float r = player_.config().radius, h = player_.config().height;

    // Sicherer Punkt: fester Boden (keine Plattform), auf dem man einen Moment ruhig steht
    bool onPlatform = false;
    for (const LevelBox& b : level.boxes) {
        if (b.tag.rfind("platform:", 0) == 0 && Level::standsOn(b, player_.position(), r)) onPlatform = true;
    }
    if (player_.grounded() && !onPlatform && player_.state() != PlayerState::Dashing && player_.state() != PlayerState::Hurt) {
        safeTimer_ += dt;
        if (safeTimer_ > 0.3f) safePosition_ = player_.position();
    } else {
        safeTimer_ = 0.0f;
    }

    if (const LevelHazard* hz = room_->hazardAt(player_.position(), r, h)) {
        if (player_.canBeHit()) {
            bool lava = hz->kind == HazardKind::Lava;
            if (damagePlayer(hz->damage, {0, 0, 0}, 0.0f) > 0.0f) {
                if (lava && !player_.dead()) {
                    player_.teleport(safePosition_, player_.yaw());
                    camera_.snapTo(player_.focusPoint());
                } else {
                    player_.body().velocity.y = 7.0f;
                    player_.body().grounded = false;
                }
            }
        }
    }

    if (player_.position().y < level.killY) {
        // Ein Sturz kostet Leben, tötet aber nie
        float dmg = std::min(15.0f, std::max(0.0f, player_.health().current() - 1.0f));
        player_.teleport(safePosition_, player_.yaw());
        if (dmg > 0.0f) {
            player_.takeDamage(dmg, {0, 0, 0}, ctx_.events);
            addFloatingText({safePosition_.x, safePosition_.y + 2.0f, safePosition_.z}, "-" + std::to_string((int)dmg), Color{255, 110, 100, 255});
        }
        camera_.snapTo(player_.focusPoint());
        camera_.addShake(0.08f);
        showToast("Abgestürzt!", 1.5f);
    }
}

void GameScene::tryDrinkFlask() {
    if (state_.flaskCharges <= 0 || player_.state() != PlayerState::Normal) return;
    if (player_.health().current() >= player_.health().max()) {
        showToast("Deine Lebenspunkte sind voll", 1.5f);
        return;
    }
    state_.flaskCharges--;
    float heal = player_.config().flaskHeal + player_.modifiers().flaskHealAdd;
    float healed = player_.healBy(heal, ctx_.events);
    Vector3 p = player_.position();
    particles_.sparkle({p.x, p.y + 0.8f, p.z}, 22, Color{110, 224, 140, 255}, 0.6f);
    addFloatingText({p.x, p.y + player_.config().height + 0.2f, p.z}, "+" + std::to_string((int)std::round(healed)), Color{130, 240, 160, 255});
}

// ---------------------------------------------------------------- Endlos-Arena

void GameScene::loadArenaBest() {
    std::string error;
    auto j = loadJsonFile((std::filesystem::path(savesDir()) / (bossRush_ ? "bossrush_best.json" : "arena_best.json")).string(), error);
    arenaBest_ = j && j->is_object() ? std::max(0, j->value("best_wave", 0)) : 0;
}

void GameScene::saveArenaBest() {
    json j;
    j["best_wave"] = arenaBest_;
    if (!writeTextFileAtomic((std::filesystem::path(savesDir()) / (bossRush_ ? "bossrush_best.json" : "arena_best.json")).string(), j.dump(2))) {
        Log::warn(LogCategory::Save, "Arena-Rekord konnte nicht gespeichert werden");
    }
}

void GameScene::spawnArenaBoss(const std::string& type) {
    const BossDef* def = bossDef(type);
    if (!def) return;
    boss_ = std::make_unique<Boss>(def, Vector3{0.0f, 0.0f, -8.0f});
    bossRewardGiven_ = false;
    bossFightActive_ = false;
    startBossFight();
}

void GameScene::updateArena(float dt) {
    if (!director_ || player_.dead()) return;
    bool bossAlive = boss_ && boss_->alive();
    ArenaDirector::Update u = director_->update(dt, aliveEnemyCount(), bossAlive);

    if (u.waveStarted) {
        int wave = director_->wave();
        ctx_.events.emit(EncounterWaveStarted{wave, 0});
        for (const SpawnRequest& s : u.spawns) spawnEnemy(s.type, s.position, 0.9f);
        if (u.boss) spawnArenaBoss(*u.boss);
        showBanner(bossRush_ ? std::format("Boss {}", wave) : (u.boss ? "Bosswelle!" : std::format("Welle {}", wave)),
                   u.boss && boss_ ? boss_->def().arenaText : std::string(), 2.2f);
    }
    if (u.waveCleared) {
        int wave = director_->wave();
        if (wave > arenaBest_) {
            arenaBest_ = wave;
            saveArenaBest();
        }
        // Zwischen den Wellen gibt es ein bisschen Leben zurück (im Boss-Rush mehr)
        player_.healBy(player_.health().max() * (bossRush_ ? 0.4f : 0.2f), ctx_.events);
        state_.flaskCharges = std::min(state_.flaskMax, state_.flaskCharges + (bossRush_ || wave % 2 == 0 ? 1 : 0));
        showBanner(bossRush_ ? std::format("Boss {} besiegt!", wave) : std::format("Welle {} geschafft!", wave), "", 1.8f);
        ctx_.events.emit(EncounterCleared{});
        if (u.offerSegen) openSegenChoice();
    }
}

// ---------------------------------------------------------------- Anzeige

void GameScene::addFloatingText(Vector3 position, std::string text, Color color) {
    floatingTexts_.push_back({position, std::move(text), color, 0.0f});
}

void GameScene::showToast(std::string text, float seconds) {
    toast_ = std::move(text);
    toastTimer_ = seconds;
}

void GameScene::showBanner(std::string title, std::string subtitle, float seconds) {
    bannerTitle_ = std::move(title);
    bannerSubtitle_ = std::move(subtitle);
    bannerTimer_ = bannerDuration_ = seconds;
}

float GameScene::bannerAlpha() const {
    if (bannerTimer_ <= 0.0f) return 0.0f;
    float elapsed = bannerDuration_ - bannerTimer_;
    return std::clamp(std::min(elapsed / 0.3f, bannerTimer_ / 0.5f), 0.0f, 1.0f);
}

void GameScene::drawWorld() {
    const Camera3D& cam = debugCamera_ ? *debugCamera_ : camera_.camera();
    const Level& level = room_->level();
    Vector3 focus = player_.position();

    // Punktlichter: die nächsten Fackeln und Feuerschalen, ein schwacher Schein um den Spieler, das Leuchten des Bosses
    std::vector<PointLight> lights = collectLights(level, focus, time_, 6);
    PointLight self;
    self.position = {focus.x, focus.y + 2.4f, focus.z};
    self.color = {0.75f, 0.68f, 0.55f};
    self.radius = 8.0f;
    lights.push_back(self);
    if (boss_ && boss_->alive()) {
        PointLight glow;
        Vector3 bp = boss_->position();
        glow.position = {bp.x, bp.y + boss_->height() * 0.55f, bp.z};
        const std::string& style = boss_->def().style;
        glow.color = style == "golem" ? Vector3{3.2f, 1.2f, 0.3f} : (style == "storm" ? Vector3{0.9f, 1.8f, 3.0f} : Vector3{2.4f, 1.6f, 0.6f});
        glow.radius = 11.0f;
        lights.push_back(glow);
    }
    ctx_.renderer.setLights(std::move(lights));

    ctx_.renderer.renderFrame(cam, focus, time_, [&]() {
        const bool shadowPass = ctx_.renderer.inShadowPass();
        RoomDrawInfo info;
        info.level = &level;
        info.room = room_.get();
        info.time = time_;
        info.checkpointActive = state_.restRoom == room_->id();
        drawRoom(ctx_.renderer, info);

        for (const Enemy& e : enemies_) e.draw(ctx_.renderer, level);
        if (boss_) boss_->draw(ctx_.renderer, level);
        if (!shadowPass) {
            for (const Shockwave& w : shockwaves_) {
                float ground = level.groundY;
                if (ground < -1000.0f) ground = w.center.y;
                int n = 44;
                for (int i = 0; i < n; i++) {
                    float a = 6.2831853f * (float)i / (float)n;
                    Vector3 p{w.center.x + std::cos(a) * w.radius, ground + 0.3f, w.center.z + std::sin(a) * w.radius};
                    ctx_.renderer.sphere(p, 0.4f, Color{255, 200, 90, 220});
                    ctx_.renderer.sphere({p.x, p.y + 0.35f, p.z}, 0.22f, Color{255, 120, 50, 200});
                }
            }
            projectiles_.draw(ctx_.renderer);
            for (const Blast& b : blasts_) {
                float k = std::clamp(1.0f - b.delay / std::max(0.01f, b.total), 0.0f, 1.0f);
                float pulse = 0.5f + 0.5f * std::sin(time_ * 22.0f);
                ctx_.renderer.cylinder({b.center.x, b.center.y + 0.04f, b.center.z}, b.radius, 0.01f,
                                       Color{b.color.r, b.color.g, b.color.b, (unsigned char)(50 + 70 * k)});
                ctx_.renderer.cylinder({b.center.x, b.center.y + 0.05f, b.center.z}, b.radius * k, 0.01f,
                                       Color{255, 220, 110, (unsigned char)(90 + 100 * pulse)});
            }
        }
        player_.draw(ctx_.renderer, level);
        if (!shadowPass) {
            particles_.draw(ctx_.renderer);
            if (debug_.showCollision) debug_.drawCollision(level);
        }
    });
}

void GameScene::drawHudAndOverlays() {
    const Camera3D& cam = debugCamera_ ? *debugCamera_ : camera_.camera();
    hud::drawEnemyOverlays(cam, enemies_);
    for (const FloatingText& t : floatingTexts_) {
        hud::drawFloatingText(cam, t.position, t.text, t.color, 1.0f - t.age / kFloatingTextLife);
    }

    {
        float low = player_.health().fraction() < 0.3f && !player_.dead() ? 0.35f + 0.15f * std::sin(time_ * 6.0f) : 0.0f;
        hud::drawDamageVignette(std::max(hurtFlash_ * 0.8f, low) * std::clamp(ctx_.settings.cameraShake, 0.0f, 1.0f));
    }

    hud::PlayerHudData d;
    d.health = &player_.health();
    d.flaskCharges = state_.flaskCharges;
    d.flaskMax = state_.flaskMax;
    d.smallKeys = state_.smallKeys;
    d.bossKey = state_.bossKey;
    d.shards = state_.shards;
    d.dashUnlocked = state_.hasAbility("dash");
    d.doubleJumpUnlocked = state_.hasAbility("double_jump");
    d.airJumpReady = player_.airJumpAvailable();
    d.dashReady = player_.dashReadyFraction();
    d.sparkReady = player_.sparkReadyFraction();
    hud::drawPlayerHud(d);

    if (boss_ && bossFightActive_ && boss_->alive()) {
        hud::drawBossBar(boss_->def().name, boss_->health().fraction(), boss_->phase(), boss_->exhausted());
    }

    // Hinweise an Schrein und verschlossenen Türen
    if (phase_ == Phase::Playing && !player_.dead()) {
        Vector2 s;
        if (room_->level().checkpoint) {
            Vector3 sh = *room_->level().checkpoint;
            if (Vector3Distance({sh.x, player_.position().y, sh.z}, player_.position()) < 2.8f &&
                hud::projectToScreen(cam, {sh.x, sh.y + 2.3f, sh.z}, s)) {
                hud::drawPrompt(std::format("[{}] Ausruhen", ctx_.input.bindingName(Action::Interact)), s);
                if (!arenaMode_ && segenAvailable()) {
                    hud::drawPrompt(std::format("[{}] Segen für {} Splitter", ctx_.input.bindingName(Action::Ability), shrineSegenCost()), {s.x, s.y + 38.0f});
                }
            }
        }
        for (size_t i = 0; i < room_->level().doors.size(); i++) {
            const LevelDoor& door = room_->level().doors[i];
            if (door.kind != DoorKind::Key || room_->doorOpen(i)) continue;
            if (Vector3Distance(door.center, player_.position()) < 4.5f &&
                hud::projectToScreen(cam, {door.center.x, door.center.y + 1.0f, door.center.z}, s)) {
                hud::drawPrompt(door.keyType == "boss_key" ? "Verschlossen: Boss-Schlüssel nötig" : "Verschlossen: Schlüssel nötig", s);
            }
        }
    }

    if (arenaMode_ && director_) hud::drawArenaInfo(director_->wave(), director_->waveKills(), arenaBest_, bossRush_);
    hud::drawBanner(bannerTitle_, bannerSubtitle_, bannerAlpha());
    if (toastTimer_ > 0.0f) {
        int w = MeasureText(toast_.c_str(), 20);
        int x = (GetScreenWidth() - w) / 2, y = GetScreenHeight() - 130;
        DrawRectangle(x - 14, y - 8, w + 28, 36, Color{20, 24, 36, 220});
        DrawText(toast_.c_str(), x, y, 20, WHITE);
    }
    if (ctx_.settings.showControlsHelp && phase_ == Phase::Playing && time_ < 40.0f) {
        // Die Hilfezeile zeigt die aktuell belegten Tasten (auch nach Änderung in den Einstellungen)
        const Input& in = ctx_.input;
        std::string moveKeys = in.bindingName(Action::MoveForward) + in.bindingName(Action::MoveLeft) + in.bindingName(Action::MoveBack) + in.bindingName(Action::MoveRight);
        if (moveKeys.size() != 4) {
            moveKeys = in.bindingName(Action::MoveForward) + "/" + in.bindingName(Action::MoveLeft) + "/" + in.bindingName(Action::MoveBack) + "/" + in.bindingName(Action::MoveRight);
        }
        std::string helpText = std::format("{} laufen · {} springen · {} Schwert · {} Funke · {} ausweichen · {} Trank", moveKeys,
                                           in.bindingName(Action::Jump), in.bindingName(Action::Attack), in.bindingName(Action::Secondary),
                                           in.bindingName(Action::Dodge), in.bindingName(Action::Heal));
        if (!arenaMode_) helpText += std::format(" · {} benutzen · {} Karte", in.bindingName(Action::Interact), in.bindingName(Action::Map));
        helpText += " · Esc Menü";
        const char* help = helpText.c_str();
        int w = MeasureText(help, 10);
        DrawRectangle((GetScreenWidth() - w) / 2 - 12, GetScreenHeight() - 26, w + 24, 20, Color{10, 12, 20, 120});
        DrawText(help, (GetScreenWidth() - w) / 2, GetScreenHeight() - 22, 10, Color{230, 235, 245, 220});
    }
}

void GameScene::draw() {
    if (!room_) {
        ClearBackground(BLACK);
        return;
    }
    drawWorld();
    drawHudAndOverlays();

    switch (phase_) {
        case Phase::Segen: hud::drawSegenChoice(segenChoices_, segenSelected_, time_, segenCost_); break;
        case Phase::Paused:
            if (showMap_) {
                drawDungeonMap(map_, state_.visitedRooms, room_ ? room_->id() : std::string(), dungeon_.name, time_);
                break;
            }
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{6, 8, 16, 150});
            if (inSettings_) settingsScreen_->draw(GetScreenHeight() * 0.5f, time_);
            else pauseMenu_.draw("Pause", GetScreenHeight() * 0.5f, time_);
            break;
        case Phase::Dying: hud::drawDeathOverlay(deathTimer_ / 0.8f); break;
        case Phase::ArenaOver: {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 6, 8, 190});
            int cx = GetScreenWidth() / 2;
            auto center = [&](const std::string& t, int y, int size, Color c) {
                int w = MeasureText(t.c_str(), size);
                DrawText(t.c_str(), cx - w / 2 + 2, y + 2, size, Color{0, 0, 0, 200});
                DrawText(t.c_str(), cx - w / 2, y, size, c);
            };
            int wave = director_ ? director_->wave() : 0;
            center(bossRush_ ? "Boss-Rush beendet" : "Arena beendet", 60, 50, Color{240, 200, 150, 255});
            int kills = director_ ? director_->waveKills() : 0;
            center(bossRush_ ? std::format("Erreichter Boss: {}", wave) : std::format("Erreichte Welle: {}", wave), 130, 30, WHITE);
            center(bossRush_ ? std::format("Besiegte Gegner: {}      Rekord: Boss {}", kills, arenaBest_)
                             : std::format("Besiegte Gegner: {}      Rekord: Welle {}", kills, arenaBest_),
                   176, 20, Color{206, 210, 228, 255});
            arenaOverMenu_.draw("Und jetzt?", GetScreenHeight() * 0.66f, time_);
            break;
        }
        case Phase::Complete: {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{6, 8, 16, 190});
            int cx = GetScreenWidth() / 2;
            auto center = [&](const std::string& t, int y, int size, Color c) {
                int w = MeasureText(t.c_str(), size);
                DrawText(t.c_str(), cx - w / 2 + 2, y + 2, size, Color{0, 0, 0, 200});
                DrawText(t.c_str(), cx - w / 2, y, size, c);
            };
            int minutes = (int)(state_.playTime / 60.0);
            center("Dungeon bezwungen!", GetScreenHeight() / 2 - 210, 50, Color{250, 226, 150, 255});
            center(dungeon_.name, GetScreenHeight() / 2 - 150, 30, Color{210, 214, 232, 255});
            center(std::format("Zeit: {} Min.   Tode: {}   Splitter: {}   Segen: {}", minutes, state_.deaths, state_.shards, state_.segen.size()),
                   GetScreenHeight() / 2 - 100, 20, WHITE);
            center(dungeon_.outro,
                   GetScreenHeight() / 2 - 66, 16, Color{190, 198, 220, 255});
            completeMenu_.draw(dungeon_.nextDungeon.empty() ? "Das Abenteuer geht weiter ..." : "Und jetzt?", GetScreenHeight() * 0.66f, time_);
            break;
        }
        case Phase::Playing: break;
    }

    if (fade_ != Fade::None) {
        float a = fade_ == Fade::Out ? fadeTimer_ / kFadeOutTime : 1.0f - fadeTimer_ / kFadeInTime;
        hud::drawFade(a);
    }

    DebugInfo info;
    info.levelName = room_->level().name;
    info.playerState = playerStateName(player_.state());
    info.playerPosition = player_.position();
    info.playerVelocity = player_.velocity();
    info.grounded = player_.grounded();
    info.litShader = ctx_.renderer.usingLitShader();
    info.boxCount = room_->level().boxes.size() + room_->level().ramps.size();
    info.enemyCount = enemies_.size();
    info.comboStep = player_.comboStep();
    debug_.draw(info);
}

std::unique_ptr<Scene> GameScene::takeNext() {
    if (nextScene_) return std::move(nextScene_);
    if (!quitToTitle_) return nullptr;
    quitToTitle_ = false;
    return std::make_unique<TitleScene>(ctx_);
}

// ---------------------------------------------------------------- Testbefehle

bool GameScene::debugCommand(const std::string& name, const std::vector<std::string>& args) {
    auto num = [&](size_t i, float def = 0.0f) {
        if (i >= args.size()) return def;
        try {
            return std::stof(args[i]);
        } catch (...) {
            return def;
        }
    };
    if (name == "teleport") {
        player_.teleport({num(0), num(1), num(2)}, args.size() > 3 ? num(3) : player_.yaw());
        camera_.snapTo(player_.focusPoint());
        if (args.size() > 3) camera_.setYaw(num(3));
        return true;
    }
    if (name == "room") {
        if (args.empty()) return true;
        loadRoom(args[0], args.size() > 1 ? args[1] : "", std::nullopt, std::nullopt);
        return true;
    }
    if (name == "map") {
        openMap(true);
        return true;
    }
    if (name == "spawn") {
        // spawn <Typ> <x> <z> [Wartezeit]: Gegner für Tests setzen
        if (args.empty()) return true;
        spawnEnemy(args[0], {num(1, 0.0f), 0.0f, num(2, 0.0f)}, num(3, 0.0f));
        return true;
    }
    if (name == "next_dungeon") {
        advanceToNextDungeon();
        return true;
    }
    if (name == "complete") {
        // Abschluss-Bildschirm des aktuellen Dungeons sofort zeigen
        complete_ = true;
        rebuildCompleteMenu();
        completeMenu_.select(0);
        phase_ = Phase::Complete;
        setMouseCaptured(false);
        return true;
    }
    if (name == "cam") {
        // cam off  oder  cam <x> <y> <z> <zielx> <ziely> <zielz> [Blickwinkel]
        if (!args.empty() && args[0] == "off") {
            debugCamera_.reset();
            return true;
        }
        Camera3D c{};
        c.position = {num(0), num(1), num(2)};
        c.target = {num(3), num(4), num(5)};
        c.up = {0, 1, 0};
        c.fovy = args.size() > 6 ? num(6) : 60.0f;
        c.projection = CAMERA_PERSPECTIVE;
        debugCamera_ = c;
        return true;
    }
    if (name == "god") {
        godMode_ = args.empty() || args[0] != "off";
        return true;
    }
    if (name == "debug") {
        debug_.visible = args.empty() || args[0] != "off";
        return true;
    }
    if (name == "collision") {
        debug_.showCollision = args.empty() || args[0] != "off";
        return true;
    }
    if (name == "give") {
        if (args.empty()) return true;
        if (args[0] == "key") state_.smallKeys += (int)num(1, 1);
        else if (args[0] == "bosskey") state_.bossKey = true;
        else if (args[0] == "shards") state_.shards += (int)num(1, 10);
        else if (args[0] == "double_jump") {
            state_.abilities.insert("double_jump");
            syncPlayerFromState();
        } else if (args[0] == "dash") {
            state_.abilities.insert("dash");
            syncPlayerFromState();
        } else if (args[0] == "shards") state_.shards += (int)num(1, 10);
        else if (args[0] == "segen" && args.size() > 1) {
            state_.segen.push_back(args[1]);
            syncPlayerFromState();
        } else if (args[0] == "heart") {
            state_.heartContainers++;
            syncPlayerFromState();
        }
        return true;
    }
    if (name == "kill_all") {
        for (Enemy& e : enemies_) {
            if (e.alive()) {
                e.takeHit(99999.0f, {0, 0, 0});
                onEnemyKilled(e);
            }
        }
        if (boss_ && boss_->targetable()) boss_->takeHit(99999.0f, {0, 0, 0});
        return true;
    }
    if (name == "boss_hp") {
        // Setzt die Bosslebenspunkte auf einen Anteil (z. B. 0.5), um Phasen zu testen
        if (boss_ && boss_->targetable()) {
            float target = num(0, 0.5f);
            float dmg = boss_->health().current() - boss_->def().maxHealth * target;
            if (dmg > 0.0f) boss_->takeHit(dmg / std::max(1.0f, boss_->exhausted() ? boss_->def().exhaustedDamageMult : 1.0f), {0, 0, 0});
        }
        return true;
    }
    if (name == "hurt") {
        // Verletzt den Spieler (ohne Unverwundbarkeit), z. B. um Tod und Wiederbeleben zu testen
        bool wasGod = godMode_;
        godMode_ = false;
        player_.godMode = false;
        player_.takeDamage(num(0, 10.0f), {0, 0, 0}, ctx_.events);
        godMode_ = wasGod;
        return true;
    }
    if (name == "heal") {
        player_.healBy(9999.0f, ctx_.events);
        state_.flaskCharges = state_.flaskMax;
        return true;
    }
    if (name == "checkpoint") {
        activateCheckpoint();
        return true;
    }
    if (name == "segen_menu") {
        openSegenChoice();
        return true;
    }
    if (name == "status") {
        Log::info(LogCategory::Core, "STATUS Raum={} Pos=({:.1f},{:.1f},{:.1f}) HP={:.0f}/{:.0f} Schluessel={} Splitter={} Segen={} Gegner={} Phase={} Zustand={} Kamera=({:.1f},{:.1f},{:.1f})",
                  room_ ? room_->id() : "-", player_.position().x, player_.position().y, player_.position().z, player_.health().current(),
                  player_.health().max(), state_.smallKeys, state_.shards, state_.segen.size(), enemies_.size(), (int)phase_,
                  playerStateName(player_.state()), camera_.camera().position.x, camera_.camera().position.y, camera_.camera().position.z);
        return true;
    }
    if (name == "save") {
        saveGame();
        return true;
    }
    return false;
}

}  // namespace aldoria
