#pragma once

#include <algorithm>
#include <format>
#include <string>

#include "core/json.h"
#include "raylib.h"

namespace aldoria::jsonutil {

// Liest optionale Zahlenlisten. Fehlt das Feld, bleibt `out` unverändert.
// `count` Zahlen sind Pflicht; bei Farben und Vektoren sind auch 4 erlaubt (der Rest wird ignoriert).
inline bool readNumbers(const json& j, const char* key, size_t count, float* out, const std::string& ctx, std::string& err) {
    auto it = j.find(key);
    if (it == j.end()) return true;
    bool ok = it->is_array() && it->size() >= count && it->size() <= (count == 3 ? 4u : count);
    if (ok) {
        for (const auto& v : *it) ok = ok && v.is_number();
    }
    if (!ok) {
        err = std::format("{}.{}: erwartet {} Zahlen", ctx, key, count);
        return false;
    }
    for (size_t i = 0; i < count; i++) out[i] = (*it)[i].get<float>();
    return true;
}

inline bool readVec3(const json& j, const char* key, Vector3& out, const std::string& ctx, std::string& err) {
    float v[3] = {out.x, out.y, out.z};
    if (!readNumbers(j, key, 3, v, ctx, err)) return false;
    out = {v[0], v[1], v[2]};
    return true;
}

inline bool readVec2(const json& j, const char* key, Vector2& out, const std::string& ctx, std::string& err) {
    float v[2] = {out.x, out.y};
    if (!readNumbers(j, key, 2, v, ctx, err)) return false;
    out = {v[0], v[1]};
    return true;
}

inline bool readColor(const json& j, const char* key, Color& out, const std::string& ctx, std::string& err) {
    float v[3] = {(float)out.r, (float)out.g, (float)out.b};
    if (!readNumbers(j, key, 3, v, ctx, err)) return false;
    auto c = [](float f) { return (unsigned char)std::clamp(f, 0.0f, 255.0f); };
    out = {c(v[0]), c(v[1]), c(v[2]), out.a};
    return true;
}

}  // namespace aldoria::jsonutil
