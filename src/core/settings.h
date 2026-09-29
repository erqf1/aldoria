#pragma once

#include <map>
#include <string>

namespace aldoria {

// Spieleinstellungen, die sich zur Laufzeit ändern und in saves/settings.json liegen.
struct Settings {
    float cameraShake = 1.0f;      // 0 = aus, 1 = normal
    float masterVolume = 0.7f;
    float musicVolume = 0.6f;
    float sfxVolume = 0.8f;
    float mouseSensitivity = 1.0f; // Faktor auf den Standardwert
    bool invertY = false;
    bool showControlsHelp = true;
    std::map<std::string, std::string> keybinds;   // vom Spieler geänderte Tasten (Aktionsname -> "key:87")
    int graphicsQuality = 2;       // 0 niedrig, 1 mittel (Schatten), 2 hoch (Schatten, Bloom)
};

// Fehlende oder unlesbare Datei ergibt die Vorgaben. Werte werden auf sinnvolle Bereiche begrenzt.
Settings loadSettings(const std::string& path);
bool saveSettings(const Settings& settings, const std::string& path);

}  // namespace aldoria
