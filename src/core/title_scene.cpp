#include "core/title_scene.h"

#include <cmath>
#include <filesystem>

#include "core/game_scene.h"
#include "core/log.h"
#include "core/math_util.h"
#include "core/paths.h"
#include "raymath.h"
#include "render/room_renderer.h"
#include "ui/settings_menu.h"

namespace aldoria {

TitleScene::TitleScene(GameContext& ctx) : ctx_(ctx), saves_(savesDir()) {
    auto result = loadLevelFromFile(dataPath("rooms/title_backdrop.json"));
    if (result.level) {
        level_ = std::move(*result.level);
    } else {
        Log::warn(LogCategory::Loading, "Titelhintergrund fehlt: {}", result.error);
        level_ = Level::makeFallback();
    }
    ctx_.renderer.setEnvironment(level_.env);
    ctx_.renderer.setTheme(level_.theme);
    if (!ctx_.input.injectionMode()) EnableCursor();

    std::vector<MenuItem> items;
    {
        MenuItem newGame;
        newGame.label = "Neues Spiel";
        newGame.onSelect = [this]() { startGame(StartMode::NewGame); };
        items.push_back(std::move(newGame));

        MenuItem cont;
        cont.label = "Weiter";
        cont.enabled = [this]() { return saves_.exists("slot1"); };
        cont.onSelect = [this]() { startGame(StartMode::Continue); };
        items.push_back(std::move(cont));

        MenuItem arena;
        arena.label = "Endlos-Arena";
        arena.onSelect = [this]() { startGame(StartMode::Arena); };
        items.push_back(std::move(arena));

        MenuItem rush;
        rush.label = "Boss-Rush";
        rush.onSelect = [this]() { startGame(StartMode::BossRush); };
        items.push_back(std::move(rush));

        MenuItem settings;
        settings.label = "Einstellungen";
        settings.onSelect = [this]() {
            inSettings_ = true;
            settingsScreen_->open();
        };
        items.push_back(std::move(settings));

        MenuItem quit;
        quit.label = "Beenden";
        quit.onSelect = [this]() { quit_ = true; };
        items.push_back(std::move(quit));
    }
    auto uiSound = [this](int k) { ctx_.audio.playUi(k); };
    menu_.sound = uiSound;
    settingsSound_ = uiSound;
    menu_.setItems(std::move(items));
    // "Weiter" ist die bessere Vorauswahl, wenn es einen Spielstand gibt
    if (saves_.exists("slot1")) menu_.select(1);

    auto saveNow = [this]() { saveSettings(ctx_.settings, (std::filesystem::path(savesDir()) / "settings.json").string()); };
    settingsScreen_ = std::make_unique<SettingsScreen>(ctx_.settings, ctx_.input, [this]() { inSettings_ = false; }, saveNow);
    settingsScreen_->setSound(settingsSound_);
}

TitleScene::~TitleScene() = default;

void TitleScene::startGame(StartMode mode) { next_ = std::make_unique<GameScene>(ctx_, mode); }

std::unique_ptr<Scene> TitleScene::takeNext() { return std::move(next_); }

void TitleScene::update(float dt) {
    (void)dt;
    float rdt = ctx_.input.injectionMode() ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.05f);
    time_ += rdt;
    if (inSettings_) {
        settingsScreen_->update(ctx_.input);
    } else {
        menu_.update(ctx_.input);
    }
}

void TitleScene::draw() {
    // Die Kamera pendelt langsam vor dem Tor; das Menü steht links, damit das Tor frei bleibt
    float angle = std::sin(time_ * 0.16f) * 0.85f;
    Vector3 target{-1.6f, 3.9f, -3.0f};
    Camera3D cam{};
    cam.position = {target.x + std::sin(angle) * 17.0f, 3.4f + std::sin(time_ * 0.3f) * 0.4f, target.z + std::cos(angle) * 17.0f};
    cam.target = target;
    cam.up = {0, 1, 0};
    cam.fovy = 50.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    ctx_.renderer.setLights(collectLights(level_, cam.target, time_, 8));
    ctx_.renderer.renderFrame(cam, cam.target, time_, [&]() {
        RoomDrawInfo info;
        info.level = &level_;
        info.time = time_;
        drawRoom(ctx_.renderer, info);
    });

    // Abdunklung links für lesbare Schrift
    DrawRectangleGradientH(0, 0, (int)(GetScreenWidth() * 0.55f), GetScreenHeight(), Color{6, 10, 18, 170}, Color{6, 10, 18, 0});
    DrawRectangleGradientV(0, 0, GetScreenWidth(), 200, Color{6, 10, 18, 120}, Color{6, 10, 18, 0});

    float menuX = GetScreenWidth() * 0.27f;
    int cx = (int)menuX;
    const char* title = "ALDORIA";
    int tw = MeasureText(title, 80);
    DrawText(title, cx - tw / 2 + 4, 44 + 4, 80, Color{0, 0, 0, 200});
    DrawText(title, cx - tw / 2, 44, 80, Color{250, 226, 150, 255});
    const char* sub = "Drei Dungeons · ein Abenteuer";
    int sw = MeasureText(sub, 28);
    DrawText(sub, cx - sw / 2 + 2, 134 + 2, 28, Color{0, 0, 0, 200});
    DrawText(sub, cx - sw / 2, 134, 28, Color{214, 220, 240, 255});

    if (inSettings_) settingsScreen_->draw(GetScreenHeight() * 0.56f, time_, menuX);
    else menu_.draw("Hauptmenü", GetScreenHeight() * 0.66f, time_, menuX);

    if (!inSettings_) {
        const char* hint = "Pfeiltasten oder WASD wählen · Enter bestätigen · Esc zurück";
        int hw = MeasureText(hint, 10);
        DrawText(hint, cx - hw / 2, GetScreenHeight() - 22, 10, Color{220, 226, 240, 200});
    }
}

bool TitleScene::debugCommand(const std::string& name, const std::vector<std::string>& args) {
    (void)args;
    if (name == "new_game") {
        startGame(StartMode::NewGame);
        return true;
    }
    if (name == "arena") {
        startGame(StartMode::Arena);
        return true;
    }
    if (name == "bossrush") {
        startGame(StartMode::BossRush);
        return true;
    }
    if (name == "continue") {
        startGame(StartMode::Continue);
        return true;
    }
    if (name == "settings") {
        inSettings_ = true;
        return true;
    }
    return false;
}

}  // namespace aldoria
