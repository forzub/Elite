#pragma once

#include <cstddef>
#include <cmath>
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

    // Support-node policy is part of the geometric search contract. The
    // implementation must not silently add a different margin or sample floor.
    double minimumSupportMarginMeters = 0.25;
    double supportMarginObstacleRadiusFactor = 0.03;

    int sphereRadialSamples = 16;
    int capsuleRadialSamples = 12;
    // Limits support nodes/search work, not the final static-scene check.
    // A failed route at this cap does not imply there is no route; retry with
    // a larger subset (0 means all obstacles) from orchestration if needed.
    std::size_t maxConsideredObstacles = 32;

    bool allowStartEscape = false;
    bool allowGoalEscape = false;
    bool simplifyLineOfSight = true;

    [[nodiscard]] bool valid() const noexcept
    {
        return
            std::isfinite(agentRadiusMeters) &&
            agentRadiusMeters >= 0.0 &&
            std::isfinite(additionalClearanceMeters) &&
            additionalClearanceMeters >= 0.0 &&
            std::isfinite(supportMarginMeters) &&
            supportMarginMeters >= 0.0 &&
            std::isfinite(minimumSupportMarginMeters) &&
            minimumSupportMarginMeters >= 0.0 &&
            std::isfinite(supportMarginObstacleRadiusFactor) &&
            supportMarginObstacleRadiusFactor >= 0.0 &&
            sphereRadialSamples >= 3 &&
            capsuleRadialSamples >= 3;
    }
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
    // valid means geometric segments clear the full supplied static scene
    // under the spherical agent envelope. It does not mean the ship can
    // physically execute the route or arrive at a terminal velocity/attitude.
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
