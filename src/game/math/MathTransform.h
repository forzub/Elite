#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::math
{

struct MathTransform
{
    // Legacy mirror. Используется старым кодом и камерой.
    // Это НЕ глобальная истина.
    glm::vec3 position {0.0f, 0.0f, 0.0f};

    // Новая настоящая координата мира.
    world::coordinates::WorldPosition worldPosition;

    glm::mat4 orientation {1.0f};

    void syncLegacyPositionFromWorld()
    {
        // ВАЖНО:
        // position больше НЕ полные глобальные метры.
        // Это только legacy mirror внутри текущей WorldPosition cell.
        // Нельзя использовать для рендера, дистанций, камеры, LOD.
        position = glm::vec3(worldPosition.localMeters);
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
    // Не использовать для глобального мира.
    // Только для старых локальных hit/damage/signal мест.

    glm::vec3 worldToLocal(const glm::vec3& world) const
    {
        const glm::vec3 relative = world - position;
        const glm::mat4 invOrientation = glm::inverse(orientation);
        return glm::vec3(invOrientation * glm::vec4(relative, 1.0f));
    }


    // LEGACY FLOAT ONLY.
    // Не использовать для глобального мира.
    // Только для старых локальных hit/damage/signal мест.

    glm::vec3 localToWorld(const glm::vec3& local) const
    {
        return glm::vec3(orientation * glm::vec4(local, 1.0f)) + position;
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