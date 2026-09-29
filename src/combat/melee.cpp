#include "combat/melee.h"

#include <cmath>

#include "core/math_util.h"

namespace aldoria {

bool meleeHits(const MeleeQuery& q, Vector3 target, float targetRadius, float targetHeight) {
    float dx = target.x - q.origin.x;
    float dz = target.z - q.origin.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist - targetRadius > q.range) return false;

    // Der Angreifer deckt etwa von den Füßen bis über den Kopf ab
    if (target.y > q.origin.y + 2.0f || target.y + targetHeight < q.origin.y - 0.2f) return false;

    if (q.halfAngle >= PI - 1e-3f) return true;
    if (dist < 1e-4f) return true;

    float angleToTarget = std::atan2(dx, dz);
    float diff = std::fabs(wrapAngle(angleToTarget - q.facingYaw));
    float slack = std::atan2(targetRadius, dist);
    return diff <= q.halfAngle + slack;
}

}  // namespace aldoria
