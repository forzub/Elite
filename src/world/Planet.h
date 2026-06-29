#pragma once

#include <glm/vec3.hpp>

#include "src/world/coordinates/WorldPosition.h"

struct Planet
{
    world::coordinates::WorldPosition worldPosition;

    // Legacy mirror. Не источник истины.
    glm::vec3 position {0.0f};

    float radius = 0.0f;

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
};