#pragma once

#include <glm/vec3.hpp>

#include "src/world/coordinates/WorldPosition.h"

struct Planet
{
    int systemId = -1;

    world::coordinates::WorldPosition worldPosition;

    float radius = 0.0f;

    void setWorldPosition(
        const world::coordinates::WorldPosition& p
    )
    {
        worldPosition = p;
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