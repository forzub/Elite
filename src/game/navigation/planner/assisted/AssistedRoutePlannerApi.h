#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation::planner::assisted
{

// Clean Assisted planner boundary.
//
// This API deliberately contains no docking, traffic-graph, BLUE-volume or
// follower concepts. It plans one free-space leg between two oriented states.
// Mandatory passages are composed outside this module.
struct BoundaryState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    double speedMps = 0.0;
};

struct VehicleLimits
{
    double radiusMeters = 0.0;
    double lengthMeters = 0.0;

    double maxSpeedMps = 0.0;
    double acceleratingMps2 = 0.0;
    double brakingMps2 = 0.0;
    double lateralMps2 = 0.0;
    double maxAngularVelocityRadPerSecond = 0.0;
    double maxAngularAccelerationRadPerSecond2 = 0.0;
};

struct GeometryPolicy
{
    // Distance that MUST remain straight before the first departure arc.
    // This is clearance, not a turn-radius budget.
    double protectedDepartureStraightMeters = 150.0;

    // Final straight before target. It is normally sized by the caller from
    // braking physics.
    double protectedArrivalStraightMeters = 700.0;

    // Ordinary Assisted geometry should remain comfortable even when a tiny
    // dynamically feasible radius exists.
    double minimumComfortRadiusMeters = 150.0;

    // Geometry is first attempted at this fraction of vehicle max speed.
    // Speed may be reduced only when the requested radius cannot be placed.
    double designSpeedFraction = 0.80;

    // Candidate discretization. These are bounded search costs, not geometry
    // semantics.
    std::size_t departurePlaneSamples = 24;
    std::size_t departureSweepSamples = 16;
    std::size_t shortlistCount = 24;

    // Display/execution sampling only; authoritative geometry remains curves.
    double denseSampleSpacingMeters = 10.0;
    double sparseGateSpacingMeters = 500.0;
};

struct CostWeights
{
    // All hard constraints are checked before cost. Cost only ranks admissible
    // candidates.
    double pathLength = 1.0;
    double wastedDeltaV = 8.0;
    double maneuverCount = 200.0;
    double traversalTime = 0.05;
};

struct Request
{
    BoundaryState start;
    BoundaryState goal;
    VehicleLimits vehicle;
    GeometryPolicy geometry;
    CostWeights cost;

    std::vector<world::navigation::NavigationObstacle> obstacles;
};

class AssistedRoutePlanner final
{
public:
    [[nodiscard]] static RoutePlan plan(const Request& request);
};

} // namespace game::navigation::planner::assisted
