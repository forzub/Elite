#pragma once

#include <glm/glm.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::damage
{

enum class DamageType
{
    Laser,
    Explosion,
    Collision,
    Radiation,
    EMP
};

struct DamageEvent
{
    DamageType type = DamageType::Collision;

    double energy = 0.0;

    // Global damage impact point.
    // Source of truth for server-side damage.
    world::coordinates::WorldPosition worldPosition;

    // Local/legacy impact point.
    // Meaning depends on processing stage:
    // - before object resolve: legacy mirror of worldPosition
    // - inside object damage resolve: object-local hit point
    glm::vec3 position {0.0f};

    glm::vec3 direction {0.0f};

    void setWorldPosition(
        const world::coordinates::WorldPosition& p
    )
    {
        worldPosition = p;
        position = world::coordinates::legacyFloatMeters(worldPosition);
    }

    void setWorldPositionMeters(
        const glm::dvec3& meters
    )
    {
        setWorldPosition(
            world::coordinates::makeWorldPositionFromMeters(meters)
        );
    }

    void setLocalPositionForResolve(
        const glm::vec3& localPosition
    )
    {
        position = localPosition;
    }
};

} // namespace game::damage