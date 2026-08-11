#pragma once

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"

namespace game::ship::physics
{

struct ShipImpulseResult
{
    glm::dvec3 deltaVelocityWorldMps {0.0};
    glm::dvec3 deltaVelocityLocalMps {0.0};

    // Components about the ship's principal body axes:
    // X/right = pitch, Y/up = yaw, Z/forward = roll.
    glm::dvec3 deltaAngularVelocityBodyRadPerSec {0.0};
};

class ShipImpulseSystem
{
public:
    // Apply an already-resolved world-space impulse at a world-space contact
    // point. This is deliberately separate from collision detection/solver:
    // the future contact solver owns J; this function owns rigid-body response.
    // Neither linear nor angular delta is clamped by pilot/control envelopes.
    static ShipImpulseResult applyImpulseAtWorldPoint(
        ShipTransform& ship,
        const ShipParams& params,
        const glm::dvec3& impulseWorldNewtonSeconds,
        const glm::dvec3& contactPointWorldMeters
    );
};

} // namespace game::ship::physics
