#include "core/settings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "core/file_util.h"
#include "core/json.h"
#include "core/log.h"

namespace aldoria {
namespace {

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

}  // namespace

Settings loadSettings(const std::string& path) {
    Settings s;
    std::string error;
    auto j = loadJsonFile(path, error);
    if (!j || !j->is_object()) return s;
    s.cameraShake = clamp01(j->value("camera_shake", s.cameraShake));
    s.masterVolume = clamp01(j->value("master_volume", s.masterVolume));
    s.musicVolume = clamp01(j->value("music_volume", s.musicVolume));
    s.sfxVolume = clamp01(j->value("sfx_volume", s.sfxVolume));
    s.mouseSensitivity = std::clamp(j->value("mouse_sensitivity", s.mouseSensitivity), 0.2f, 3.0f);
    s.invertY = j->value("invert_y", s.invertY);
    s.showControlsHelp = j->value("show_controls_help", s.showControlsHelp);
    s.graphicsQuality = std::clamp(j->value("graphics_quality", s.graphicsQuality), 0, 2);
    if (auto it = j->find("keybinds"); it != j->end() && it->is_object()) {
        for (auto k = it->begin(); k != it->end(); ++k) {
            if (k.value().is_string()) s.keybinds[k.key()] = k.value().get<std::string>();
        }
    }
    return s;
}

bool saveSettings(const Settings& s, const std::string& path) {
    json j;
    j["camera_shake"] = s.cameraShake;
    j["master_volume"] = s.masterVolume;
    j["music_volume"] = s.musicVolume;
    j["sfx_volume"] = s.sfxVolume;
    j["mouse_sensitivity"] = s.mouseSensitivity;
    j["invert_y"] = s.invertY;
    j["show_controls_help"] = s.showControlsHelp;
    j["graphics_quality"] = s.graphicsQuality;
    json binds = json::object();
    for (const auto& [name, code] : s.keybinds) binds[name] = code;
    j["keybinds"] = binds;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << j.dump(2);
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        Log::warn(LogCategory::Save, "Einstellungen konnten nicht gespeichert werden: {}", ec.message());
        return false;
    }
    return true;
}

}  // namespace aldoria
