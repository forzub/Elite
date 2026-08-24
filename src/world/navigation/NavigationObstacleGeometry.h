#pragma once

#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace world::navigation
{

double navigationObstacleInflationMeters(
    const NavigationObstacle& obstacle,
    double agentRadiusMeters,
    double additionalClearanceMeters = 0.0
) noexcept;

bool pointInsideNavigationObstacle(
    const glm::dvec3& pointMeters,
    const NavigationObstacle& obstacle,
    double agentRadiusMeters = 0.0,
    double additionalClearanceMeters = 0.0
) noexcept;

bool segmentIntersectsNavigationObstacle(
    const glm::dvec3& startMeters,
    const glm::dvec3& endMeters,
    const NavigationObstacle& obstacle,
    double agentRadiusMeters = 0.0,
    double additionalClearanceMeters = 0.0
) noexcept;

bool segmentClearOfNavigationObstacles(
    const glm::dvec3& startMeters,
    const glm::dvec3& endMeters,
    const std::vector<NavigationObstacle>& obstacles,
    double agentRadiusMeters = 0.0,
    double additionalClearanceMeters = 0.0,
    std::string_view ignoredObstacleId = {}
) noexcept;

} // namespace world::navigation
