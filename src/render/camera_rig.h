#pragma once

#include "raylib.h"
#include "world/level.h"

namespace aldoria {

// Dritte-Person-Kamera mit Kollision plus freie Flugkamera für die Entwicklung.
// Blickwinkel: yaw dreht um die Hochachse, pitch > 0 schaut nach unten.
class CameraRig {
public:
    CameraRig();

    // `look` in Radiant (aus Input::lookDelta)
    void updateFollow(float dt, Vector2 look, Vector3 focus, const Level& level);
    // move: x = rechts, y = hoch, z = vorwärts
    void updateFly(float dt, Vector2 look, Vector3 move, float speed);

    void setFly(bool fly);
    bool fly() const { return fly_; }
    void snapTo(Vector3 focus);

    // Kurzes Wackeln bei Treffern. `scale` kommt aus den Einstellungen (0 = aus).
    void addShake(float magnitude) { shake_ = shake_ > magnitude ? shake_ : magnitude; }
    void setShakeScale(float scale) { shakeScale_ = scale; }

    float yaw() const { return yaw_; }
    void setYaw(float yaw) { yaw_ = yaw; }
    // Kamera für das Zeichnen (inklusive Wackeln)
    const Camera3D& camera() const { return render_; }

private:
    void finishFrame(float dt);

    Camera3D camera_{};
    Camera3D render_{};
    float yaw_ = 3.14159265f;
    float pitch_ = 0.35f;
    float distance_ = 6.0f;
    float currentDistance_ = 6.0f;
    Vector3 smoothFocus_{0, 0, 0};
    float shake_ = 0.0f;
    float shakeScale_ = 1.0f;
    bool fly_ = false;
    bool focusValid_ = false;
};

}  // namespace aldoria
