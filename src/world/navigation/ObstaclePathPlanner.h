#pragma once

#include <vector>
#include <glm/vec3.hpp>

#include <glm/glm.hpp>

#include "src/world/navigation/ObstacleAvoidance.h"
#include "src/world/navigation/SmallCraftNavigation.h"

namespace world::navigation
{

struct ObstaclePathPlannerParams
{
    float clearance = 3.0f;
    float maxBypassDistance = 40.0f;
    int radialSamples = 12;
    bool allowTwoPointBypass = true;

    int maxConsideredObstacles = 24;
    float endpointIgnoreRadius = 1.5f;
    float corridorRadius = 80.0f;
};

std::vector<SmallCraftWaypoint> buildObstacleAwarePath(
    const glm::vec3& start,
    const glm::vec3& target,
    const std::vector<NavigationObstacle>& obstacles,
    const ObstaclePathPlannerParams& params,
    SmallCraftWaypointKind finalKind
);

bool debugSegmentBlockedByNavigationObstacles(
    const glm::vec3& a,
    const glm::vec3& b,
    const std::vector<NavigationObstacle>& obstacles,
    float clearance
);

} // namespace world::navigation