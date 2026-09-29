#pragma once

#include <string>

namespace aldoria {

struct AppOptions {
    std::string scriptPath;        // --script <Datei>: Testskript abspielen (Autopilot)
    std::string shotsDir = ".";    // --shots <Ordner>: Ziel für Screenshots aus Skripten
    bool mute = false;             // --mute: keine Töne
    bool unmute = false;           // --unmute: Ton auch in Testskripten (sonst sind Skripte stumm)
    float masterVolume = -1.0f;    // --volume <0..1>: überschreibt die Gesamtlautstärke (nur Testläufe)
    float maxSeconds = 300.0f;     // --max-seconds <n>: Sicherheitsgrenze für Skripte
    bool windowed = true;
};

// Besitzt Fenster, gemeinsame Dienste und die aktive Szene und führt die Hauptschleife aus.
class App {
public:
    explicit App(AppOptions options = {}) : options_(std::move(options)) {}
    int run();

private:
    AppOptions options_;
};

}  // namespace aldoria
