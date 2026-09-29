#pragma once

#include <string>
#include <vector>

#include "core/input.h"
#include "raylib.h"

namespace aldoria {

struct AutopilotCommand {
    std::string name;
    std::vector<std::string> args;
};

// Spielt ein Skript aus Eingaben und Befehlen ab. So kann ich das Spiel automatisch durchspielen,
// prüfen und Screenshots machen, ohne Tasten an das Betriebssystem zu schicken.
//
// Skriptformat (eine Anweisung pro Zeile, # beginnt einen Kommentar):
//   wait 1.5                 Sekunden warten
//   hold forward+sprint 2    Aktionen gedrückt halten (Namen siehe actionFromName)
//   press attack             Aktion einen Frame lang drücken
//   look 0.5 0.1             Kamera drehen (yaw, pitch in Radiant)
//   <sonst>                  Befehl an App/Szene, z. B. "shot name", "teleport 1 0 2", "quit"
class Autopilot {
public:
    bool loadFromFile(const std::string& path, std::string& error);
    bool loadFromString(const std::string& text, std::string& error);

    bool finished() const { return index_ >= steps_.size(); }
    size_t stepCount() const { return steps_.size(); }

    // Einmal pro Frame vor Input::update() aufrufen. Setzt die eingespeisten Aktionen und liefert Befehle.
    void update(float dt, Input& input, std::vector<AutopilotCommand>& commands);

private:
    enum class StepType { Wait, Hold, Press, Look, Command };
    struct Step {
        StepType type = StepType::Wait;
        std::vector<Action> actions;
        float duration = 0.0f;
        Vector2 look{0, 0};
        AutopilotCommand command;
    };

    std::vector<Step> steps_;
    size_t index_ = 0;
    float elapsed_ = 0.0f;
};

}  // namespace aldoria
