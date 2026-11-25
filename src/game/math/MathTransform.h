#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::math
{

struct MathTransform
{
    world::coordinates::WorldPosition worldPosition;

    // (the legacy float mirror `position` was removed 2026-11-25 - the duplicated-truth pass: worldPosition is the one
    // owner of the position; wire schema 77)

    glm::mat4 orientation {1.0f};

    void syncLegacyPositionFromWorld()
    {
        // ВАЖНО:
        // position больше НЕ полные глобальные метры.
        // Это только legacy mirror внутри текущей WorldPosition cell.
        // Нельзя использовать для рендера, дистанций, камеры, LOD.
        // nothing to mirror any more (kept so the callers stay unchanged)
    }

    void setWorldPosition(const world::coordinates::WorldPosition& p)
    {
        worldPosition = p;
        syncLegacyPositionFromWorld();
    }

    void setWorldPositionMeters(const glm::dvec3& meters)
    {
        worldPosition = world::coordinates::makeWorldPositionFromMeters(meters);
        syncLegacyPositionFromWorld();
    }

    void addWorldMeters(const glm::dvec3& deltaMeters)
    {
        world::coordinates::addMeters(worldPosition, deltaMeters);
        syncLegacyPositionFromWorld();
    }


glm::dvec3 fullWorldMeters() const
{
    return world::coordinates::fullMeters(worldPosition);
}

glm::vec3 localCellMeters() const
{
    return glm::vec3(worldPosition.localMeters);
}

glm::vec3 worldPositionToLocalCell(
    const world::coordinates::WorldPosition& p
) const
{
    return world::coordinates::relativeMetersFloat(
        p,
        worldPosition
    );
}


glm::dvec3 worldPositionToLocalMetersDouble(
    const world::coordinates::WorldPosition& p
) const
{
    return world::coordinates::fullMeters(p)
         - world::coordinates::fullMeters(worldPosition);
}

float distanceToWorldPosition(
    const world::coordinates::WorldPosition& p
) const
{
    return glm::length(
        worldPositionToLocalCell(p)
    );
}

float distanceSquaredToWorldPosition(
    const world::coordinates::WorldPosition& p
) const
{
    const glm::vec3 local =
        worldPositionToLocalCell(p);

    return glm::dot(local, local);
}






world::coordinates::WorldPosition localCellToWorldPosition(
    const glm::vec3& local
) const
{
    return world::coordinates::translated(
        worldPosition,
        glm::dvec3(local)
    );
}







    glm::vec3 forward() const
    {
        return glm::normalize(glm::vec3(orientation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    }

    glm::vec3 right() const
    {
        return glm::normalize(glm::vec3(orientation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
    }

    glm::vec3 up() const
    {
        return glm::normalize(glm::vec3(orientation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
    }

    // LEGACY FLOAT ONLY.
    // Использовать только для owner-local hit/damage кода,
    // где входные точки уже находятся в той же локальной float-системе.
    // Запрещено использовать для глобальных world-distance,
    // navigation, camera, render, radar, LOD.

    glm::vec3 worldToLocal(const glm::vec3& world) const
    {
        const glm::vec3 relative = world - glm::vec3(worldPosition.localMeters);   // the cell-local float frame
        const glm::mat4 invOrientation = glm::inverse(orientation);
        return glm::vec3(invOrientation * glm::vec4(relative, 1.0f));
    }


    // LEGACY FLOAT ONLY.
    // Использовать только для owner-local hit/damage кода,
    // где входные точки уже находятся в той же локальной float-системе.
    // Запрещено использовать для глобальных world-distance,
    // navigation, camera, render, radar, LOD.

    glm::vec3 localToWorld(const glm::vec3& local) const
    {
        return glm::vec3(orientation * glm::vec4(local, 1.0f)) + glm::vec3(worldPosition.localMeters);
    }

    glm::vec3 directionToLocal(const glm::vec3& dir) const
    {
        const glm::mat4 invOrientation = glm::inverse(orientation);
        return glm::vec3(invOrientation * glm::vec4(dir, 0.0f));
    }

    glm::vec3 directionToWorld(const glm::vec3& dir) const
    {
        return glm::vec3(orientation * glm::vec4(dir, 0.0f));
    }
};

} // namespace game::math