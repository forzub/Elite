#include "ObstacleAvoidance.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/gtx/norm.hpp>

namespace world::navigation
{

bool segmentIntersectsSphere(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& center,
    float radius
)
{
    const glm::vec3 ab = b - a;
    const float abLen2 = glm::length2(ab);

    if (abLen2 <= 0.000001f)
        return glm::length2(a - center) <= radius * radius;

    const float t = glm::clamp(
        glm::dot(center - a, ab) / abLen2,
        0.0f,
        1.0f
    );

    const glm::vec3 closest = a + ab * t;
    return glm::length2(closest - center) <= radius * radius;
}

std::vector<SmallCraftWaypoint> buildSimpleAvoidancePath(
    const glm::vec3& start,
    const glm::vec3& target,
    const std::vector<NavigationObstacle>& obstacles,
    float clearance,
    SmallCraftWaypointKind finalKind,
    float ignoreObstaclesNearTargetRadius
)
{
    std::vector<SmallCraftWaypoint> path;

    glm::vec3 current = start;

    for (const auto& obstacle : obstacles)
{
    if (ignoreObstaclesNearTargetRadius > 0.0f)
    {
        const float ignoreRadius =
            obstacle.radius + ignoreObstaclesNearTargetRadius;

        if (glm::length2(obstacle.center - target) <= ignoreRadius * ignoreRadius)
            continue;
    }

    const float safeRadius = obstacle.radius + clearance;

        if (!segmentIntersectsSphere(
                current,
                target,
                obstacle.center,
                safeRadius))
        {
            continue;
        }

        glm::vec3 pathDir = target - current;

        if (glm::length2(pathDir) < 0.000001f)
            pathDir = glm::vec3(0.0f, 0.0f, -1.0f);
        else
            pathDir = glm::normalize(pathDir);

        glm::vec3 fromObstacle = current - obstacle.center;

        if (glm::length2(fromObstacle) < 0.000001f)
            fromObstacle = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            fromObstacle = glm::normalize(fromObstacle);

        glm::vec3 side = glm::cross(pathDir, fromObstacle);

        if (glm::length2(side) < 0.000001f)
            side = glm::cross(pathDir, glm::vec3(0.0f, 1.0f, 0.0f));

        if (glm::length2(side) < 0.000001f)
            side = glm::cross(pathDir, glm::vec3(1.0f, 0.0f, 0.0f));

        side = glm::normalize(side);

        const glm::vec3 aroundPoint =
            obstacle.center +
            fromObstacle * safeRadius +
            side * safeRadius * 0.65f;

        path.push_back({
            aroundPoint,
            SmallCraftWaypointKind::Transit
        });

        current = aroundPoint;
    }

    path.push_back({
        target,
        finalKind
    });

    return path;
}

} // namespace world::navigation