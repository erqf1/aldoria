#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/game_context.h"
#include "core/game_scene.h"
#include "core/save_manager.h"
#include "core/scene.h"
#include "ui/menu.h"
#include "ui/settings_menu.h"
#include "world/level.h"

namespace aldoria {

// Titelbildschirm mit kreisender Kamera über einem Raum und dem Hauptmenü.
class TitleScene : public Scene {
public:
    explicit TitleScene(GameContext& ctx);
    ~TitleScene() override;

    void update(float dt) override;
    void draw() override;
    bool debugCommand(const std::string& name, const std::vector<std::string>& args) override;
    std::unique_ptr<Scene> takeNext() override;
    bool wantsQuit() const override { return quit_; }

private:
    void startGame(StartMode mode);

    GameContext& ctx_;
    SaveManager saves_;
    Level level_;
    Menu menu_;
    std::unique_ptr<SettingsScreen> settingsScreen_;
    std::function<void(int)> settingsSound_;
    bool inSettings_ = false;
    bool quit_ = false;
    float time_ = 0.0f;
    std::unique_ptr<Scene> next_;
};

}  // namespace aldoria
