#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace world::navigation
{

struct GeometricPathPlannerParams
{
    double agentRadiusMeters = 0.0;
    double additionalClearanceMeters = 0.0;
    double supportMarginMeters = 2.0;

    int sphereRadialSamples = 16;
    int capsuleRadialSamples = 12;
    std::size_t maxConsideredObstacles = 32;

    bool allowStartEscape = false;
    bool allowGoalEscape = false;
    bool simplifyLineOfSight = true;
};

struct GeometricPathRequest
{
    glm::dvec3 startMeters {0.0};
    glm::dvec3 goalMeters {0.0};
    std::vector<NavigationObstacle> obstacles;
    GeometricPathPlannerParams params;
};

struct GeometricPathResult
{
    bool valid = false;
    bool obstacleDetourUsed = false;
    bool startEscaped = false;
    bool goalEscaped = false;
    std::string message;
    double lengthMeters = 0.0;
    std::vector<glm::dvec3> pointsMeters;
};

class GeometricPathPlanner
{
public:
    static GeometricPathResult plan(const GeometricPathRequest& request);
};

} // namespace world::navigation
