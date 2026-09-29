#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"

namespace aldoria {

inline float wrapAngle(float a) {
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

// Dreht `current` um höchstens `maxStep` Richtung `target` (kürzester Weg)
inline float turnToward(float current, float target, float maxStep) {
    float d = wrapAngle(target - current);
    if (std::fabs(d) <= maxStep) return target;
    return current + (d > 0 ? maxStep : -maxStep);
}

inline float approach(float current, float target, float maxDelta) {
    if (std::fabs(target - current) <= maxDelta) return target;
    return current + (target > current ? maxDelta : -maxDelta);
}

// Farbe aus Werten 0..1
inline Color toColor(Vector3 v) {
    auto c = [](float f) { return (unsigned char)(std::clamp(f, 0.0f, 1.0f) * 255.0f); };
    return Color{c(v.x), c(v.y), c(v.z), 255};
}

}  // namespace aldoria
