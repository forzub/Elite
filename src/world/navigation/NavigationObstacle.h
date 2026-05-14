#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace world::navigation
{

enum class NavigationObstacleShape : uint8_t
{
    Sphere = 0,
    Box = 1
};

struct NavigationObstacle
{
    NavigationObstacleShape shape =
        NavigationObstacleShape::Sphere;

    glm::vec3 center {0.0f};

    // Sphere
    float radius = 1.0f;

    // Box / OBB
    glm::vec3 halfExtents {1.0f};

    // World-space orientation basis.
    glm::mat3 rotation {1.0f};

    uint32_t entityId = 0;
};

} // namespace world::navigation