#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/SmallCraftNavigation.h"

namespace world::navigation
{

bool segmentIntersectsSphere(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& center,
    float radius
);

std::vector<SmallCraftWaypoint> buildSimpleAvoidancePath(
    const glm::vec3& start,
    const glm::vec3& target,
    const std::vector<NavigationObstacle>& obstacles,
    float clearance = 3.0f,
    SmallCraftWaypointKind finalKind = SmallCraftWaypointKind::WorkPoint,
    float ignoreObstaclesNearTargetRadius = 0.0f
);

} // namespace world::navigation