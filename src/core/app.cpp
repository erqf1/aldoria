#include "core/app.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>

#include "core/autopilot.h"
#include "core/game_context.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/title_scene.h"
#include "raylib.h"
#include "rlgl.h"

namespace aldoria {
namespace {

constexpr float kBaseMouseSensitivity = 0.0028f;

// Speichert das aktuelle Bild. Muss nach dem Zeichnen und vor EndDrawing() laufen.
void saveScreenshot(const std::string& path) {
    rlDrawRenderBatchActive();  // ausstehende Zeichenbefehle in den Puffer schreiben
    Image image = LoadImageFromScreen();
    if (ExportImage(image, path.c_str())) Log::info(LogCategory::Core, "Screenshot gespeichert: {}", path);
    else Log::error(LogCategory::Core, "Screenshot konnte nicht gespeichert werden: {}", path);
    UnloadImage(image);
}

}  // namespace

int App::run() {
    Log::openFile((std::filesystem::path(GetApplicationDirectory()) / "aldoria.log").string());
    Log::info(LogCategory::Core, "Aldoria startet, Datenordner: {}", dataDir());

    Autopilot autopilot;
    const bool scripted = !options_.scriptPath.empty();
    if (scripted) {
        std::string error;
        if (!autopilot.loadFromFile(options_.scriptPath, error)) {
            Log::error(LogCategory::Core, "{}", error);
            return 2;
        }
        Log::info(LogCategory::Core, "Testskript geladen: {} ({} Schritte)", options_.scriptPath, autopilot.stepCount());
        std::filesystem::create_directories(options_.shotsDir);
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Aldoria");
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);  // Esc gehört dem Spiel (Pause-Menü)

    GameContext ctx;
    ctx.mute = options_.mute || (scripted && !options_.unmute);
    ctx.input.setInjectionMode(scripted);
    const std::string settingsPath = (std::filesystem::path(savesDir()) / "settings.json").string();
    // Testläufe nutzen immer die Standardeinstellungen, damit sie reproduzierbar bleiben
    if (!scripted) ctx.settings = loadSettings(settingsPath);
    ctx.input.importBindings(ctx.settings.keybinds);
    ctx.input.onRebound = [&ctx, settingsPath]() {
        ctx.settings.keybinds = ctx.input.exportBindings();
        saveSettings(ctx.settings, settingsPath);
    };
    if (options_.masterVolume >= 0.0f) ctx.settings.masterVolume = options_.masterVolume;
    ctx.renderer.init(dataDir());
    if (!ctx.mute && ctx.audio.init()) ctx.audio.attach(ctx.events);

    std::unique_ptr<Scene> scene = std::make_unique<TitleScene>(ctx);

    const auto startTime = std::chrono::steady_clock::now();
    bool quit = false;
    std::string pendingShot;

    while (!quit && !WindowShouldClose()) {
        // Skripte laufen mit festem Zeitschritt, damit Ergebnisse reproduzierbar sind
        float realDt = scripted ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.05f);

        if (scripted) {
            std::vector<AutopilotCommand> commands;
            autopilot.update(realDt, ctx.input, commands);
            for (const AutopilotCommand& c : commands) {
                if (c.name == "quit") quit = true;
                else if (c.name == "shot" && !c.args.empty()) pendingShot = c.args[0];
                else if (!scene->debugCommand(c.name, c.args))
                    Log::warn(LogCategory::Core, "Unbekannter Skriptbefehl '{}'", c.name);
            }
            if (autopilot.finished() && pendingShot.empty()) quit = true;
            double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
            if (elapsed > options_.maxSeconds) {
                Log::error(LogCategory::Core, "Skript hat die Zeitgrenze von {} s überschritten", options_.maxSeconds);
                quit = true;
            }
        }

        if (!scripted && IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        ctx.renderer.setQuality(ctx.settings.graphicsQuality);
        ctx.input.mouseSensitivity = kBaseMouseSensitivity * ctx.settings.mouseSensitivity;
        ctx.input.invertY = ctx.settings.invertY;
        ctx.input.update();
        ctx.audio.update(realDt, ctx.settings);
        float dt = ctx.time.advance(realDt);
        scene->update(dt);
        if (scene->wantsQuit()) quit = true;

        BeginDrawing();
        scene->draw();
        if (!pendingShot.empty()) {
            saveScreenshot((std::filesystem::path(options_.shotsDir) / (pendingShot + ".png")).string());
            pendingShot.clear();
        }
        EndDrawing();

        if (auto next = scene->takeNext()) scene = std::move(next);
    }

    if (!scripted) saveSettings(ctx.settings, settingsPath);
    scene.reset();
    ctx.audio.shutdown();
    ctx.renderer.shutdown();
    CloseWindow();
    Log::info(LogCategory::Core, "Aldoria beendet");
    return 0;
}

}  // namespace aldoria
