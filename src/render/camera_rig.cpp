#include "render/camera_rig.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace aldoria {
namespace {

constexpr float kMinPitch = -0.5f;
constexpr float kMaxPitch = 1.25f;
constexpr float kCameraMargin = 0.3f;

Vector3 viewDirection(float yaw, float pitch) {
    return {std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
}

float randomSigned() { return (float)GetRandomValue(-100, 100) / 100.0f; }

}  // namespace

CameraRig::CameraRig() {
    camera_.up = {0, 1, 0};
    camera_.fovy = 65.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    render_ = camera_;
}

void CameraRig::snapTo(Vector3 focus) {
    smoothFocus_ = focus;
    focusValid_ = true;
    currentDistance_ = distance_;
}

void CameraRig::setFly(bool fly) {
    fly_ = fly;
    // Beim Zurückschalten neu an den Spieler binden
    if (!fly) focusValid_ = false;
}

void CameraRig::finishFrame(float dt) {
    shake_ *= std::exp(-9.0f * dt);
    render_ = camera_;
    float magnitude = shake_ * shakeScale_;
    if (magnitude > 0.001f) {
        Vector3 offset{randomSigned() * magnitude, randomSigned() * magnitude, randomSigned() * magnitude};
        render_.position = Vector3Add(render_.position, offset);
        render_.target = Vector3Add(render_.target, offset);
    }
}

void CameraRig::updateFollow(float dt, Vector2 look, Vector3 focus, const Level& level) {
    yaw_ -= look.x;
    pitch_ = std::clamp(pitch_ + look.y, kMinPitch, kMaxPitch);
    if (!focusValid_) snapTo(focus);

    // Kamera folgt weich; Bewegung des Spielers soll nicht ruckeln
    float k = 1.0f - std::exp(-16.0f * dt);
    smoothFocus_ = Vector3Lerp(smoothFocus_, focus, k);

    // Richtung vom Fokus zur Kamera (Länge 1)
    Vector3 back = Vector3Negate(viewDirection(yaw_, pitch_));

    float wanted = distance_;
    float hit;
    if (level.raycast(smoothFocus_, back, distance_ + kCameraMargin, hit)) {
        wanted = std::max(0.6f, hit - kCameraMargin);
    }
    // Durch Türöffnungen darf die Kamera den Raum nicht verlassen (sonst sieht man den Spieler von draußen nicht mehr)
    if (level.shellSize.x > 0.0f && level.shellSize.y > 0.0f) {
        float halfX = level.shellSize.x * 0.5f - kCameraMargin, halfZ = level.shellSize.y * 0.5f - kCameraMargin;
        auto limit = [&](float focusCoord, float dir, float half) {
            if (dir > 1e-4f) return (half - focusCoord) / dir;
            if (dir < -1e-4f) return (-half - focusCoord) / dir;
            return 1e9f;
        };
        float roomLimit = std::min(limit(smoothFocus_.x, back.x, halfX), limit(smoothFocus_.z, back.z, halfZ));
        wanted = std::min(wanted, std::max(0.6f, roomLimit));
    }
    // Wände schieben die Kamera sofort heran, danach gleitet sie sanft wieder heraus
    if (wanted < currentDistance_) currentDistance_ = wanted;
    else currentDistance_ = std::min(wanted, currentDistance_ + 12.0f * dt);

    Vector3 position = Vector3Add(smoothFocus_, Vector3Scale(back, currentDistance_));
    // Nie unter dem Boden oder einer Rampe
    position.y = std::max(position.y, level.groundHeight(position, 0.1f, position.y) + 0.25f);
    camera_.position = position;
    camera_.target = smoothFocus_;
    finishFrame(dt);
}

void CameraRig::updateFly(float dt, Vector2 look, Vector3 move, float speed) {
    yaw_ -= look.x;
    pitch_ = std::clamp(pitch_ + look.y, -1.5f, 1.5f);

    Vector3 forward = viewDirection(yaw_, pitch_);
    Vector3 right{-std::cos(yaw_), 0.0f, std::sin(yaw_)};
    Vector3 delta = Vector3Add(Vector3Add(Vector3Scale(forward, move.z), Vector3Scale(right, move.x)),
                               Vector3{0.0f, move.y, 0.0f});
    camera_.position = Vector3Add(camera_.position, Vector3Scale(delta, speed * dt));
    camera_.target = Vector3Add(camera_.position, forward);
    finishFrame(dt);
}

}  // namespace aldoria
