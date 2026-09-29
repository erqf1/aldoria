#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "raymath.h"

namespace aldoria {

constexpr float kPi = 3.14159265f;

// Neigung nach vorn (+z) um die Seitenachse durch `pivot`
inline Vector3 pitchAround(Vector3 v, Vector3 pivot, float a) {
    float c = std::cos(a), s = std::sin(a);
    Vector3 d = Vector3Subtract(v, pivot);
    return {v.x, pivot.y + d.y * c - d.z * s, pivot.z + d.y * s + d.z * c};
}

// Kippen zur Seite um die Vorwärtsachse durch `pivot`
inline Vector3 rollAround(Vector3 v, Vector3 pivot, float a) {
    float c = std::cos(a), s = std::sin(a);
    Vector3 d = Vector3Subtract(v, pivot);
    return {pivot.x + d.x * c - d.y * s, pivot.y + d.x * s + d.y * c, v.z};
}

// Zwei-Knochen-Kette (Bein, Arm): Wurzel und Ziel sind vorgegeben, das Gelenk knickt in Richtung `bend` aus
inline void twoBone(Vector3 root, Vector3 target, float l1, float l2, Vector3 bend, Vector3& mid, Vector3& end) {
    Vector3 d = Vector3Subtract(target, root);
    float dist = Vector3Length(d);
    float maxReach = (l1 + l2) * 0.998f;
    if (dist > maxReach) {
        d = Vector3Scale(d, maxReach / dist);
        dist = maxReach;
    }
    if (dist < 0.06f) {
        end = root;
        mid = Vector3Add(root, Vector3Scale(bend, 0.1f));
        return;
    }
    Vector3 dir = Vector3Scale(d, 1.0f / dist);
    end = Vector3Add(root, d);
    float a = (l1 * l1 - l2 * l2 + dist * dist) / (2.0f * dist);
    float hh = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
    Vector3 b = Vector3Subtract(bend, Vector3Scale(dir, Vector3DotProduct(bend, dir)));
    float bl = Vector3Length(b);
    b = bl > 1e-4f ? Vector3Scale(b, 1.0f / bl) : Vector3{0, 0, 1};
    mid = Vector3Add(Vector3Add(root, Vector3Scale(dir, a)), Vector3Scale(b, hh));
}

}  // namespace aldoria
