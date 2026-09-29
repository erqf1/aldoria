#include "world/kinematic_body.h"

#include <algorithm>

#include "raymath.h"

namespace aldoria {

float KinematicBody::step(float dt, const Level& level, float gravity, float maxFallSpeed) {
    velocity.y = std::max(velocity.y - gravity * dt, -maxFallSpeed);

    float previousFeet = position.y;
    bool wasGrounded = grounded;
    position = Vector3Add(position, Vector3Scale(velocity, dt));
    level.resolveHorizontal(position, radius, height);

    // Mit dem höheren Fußwert von vorher/nachher suchen, damit man schnell fallend nicht durch Kisten rutscht
    float ground = level.groundHeight(position, radius, std::max(previousFeet, position.y));
    bool landing = position.y <= ground + 0.001f && velocity.y <= 0.0f;
    bool stayingOnSteps = wasGrounded && velocity.y <= 0.0f && position.y - ground <= Level::kStepHeight;

    float impact = 0.0f;
    if (landing || stayingOnSteps) {
        if (!wasGrounded) impact = -velocity.y;
        position.y = ground;
        velocity.y = 0.0f;
        grounded = true;
    } else {
        grounded = false;
    }
    return impact;
}

}  // namespace aldoria
