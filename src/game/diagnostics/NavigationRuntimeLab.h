#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace game::diagnostics
{

// Stage-12 live NavigationWorld proving actor.
// Enabled only in the existing diagnostic hub scene.
inline constexpr bool NavigationRuntimeLabEnabled = true;
inline constexpr std::uint64_t NavigationRuntimeLabInstanceId = 9030;
inline constexpr const char* NavigationRuntimeLabLabel =
    "NAVIGATION V2 RUNTIME LAB";
inline constexpr const char* NavigationRuntimeLabHubId =
    "earth_orbital_hub";
inline constexpr const char* NavigationRuntimeLabObstacleLabel =
    "NAV STRESS CUBE 08";

// Hub ReferenceFrame uses tactical local axes:
//   X = prograde, Y = radial, Z = normal.
//
// The stress objects are authored in visual hub axes:
//   X = normal, Y = radial, Z = -prograde.
//
// This start/goal pair therefore corresponds to visual:
//   start = { 975, -1300, -8000 }
//   goal  = { 975, -1300,  1000 }
//
// The straight line crosses NAV STRESS CUBE 08 at
// visual { 975, -1300, -4900 }, forcing the live local planner to react.
inline const glm::dvec3 NavigationRuntimeLabStartVisualLocalMeters {
    975.0,
    -1300.0,
    -8000.0
};

inline const glm::dvec3 NavigationRuntimeLabGoalVisualLocalMeters {
    975.0,
    -1300.0,
    1000.0
};

inline const glm::dvec3 NavigationRuntimeLabObstacleVisualLocalMeters {
    975.0,
    -1300.0,
    -4900.0
};

inline const glm::dvec3 NavigationRuntimeLabStartTacticalLocalMeters {
    -NavigationRuntimeLabStartVisualLocalMeters.z,
    NavigationRuntimeLabStartVisualLocalMeters.y,
    NavigationRuntimeLabStartVisualLocalMeters.x
};

inline const glm::dvec3 NavigationRuntimeLabGoalTacticalLocalMeters {
    -NavigationRuntimeLabGoalVisualLocalMeters.z,
    NavigationRuntimeLabGoalVisualLocalMeters.y,
    NavigationRuntimeLabGoalVisualLocalMeters.x
};

inline constexpr double NavigationRuntimeLabMaximumSpeedMps = 60.0;
inline constexpr double NavigationRuntimeLabArrivalRadiusMeters = 20.0;
inline constexpr double NavigationRuntimeLabWorkspaceHalfExtentMeters = 12000.0;

struct NavigationRuntimeLabObservation
{
    bool valid = false;

    std::uint32_t shipEntityId = 0;
    std::uint32_t obstacleEntityId = 0;

    std::uint64_t planCount = 0;
    std::uint64_t executionCount = 0;
    std::uint64_t lastIntentRevision = 0;
    std::uint8_t lastPlannerStatus = 0xffu;

    bool obstacleCandidateSeen = false;
    bool obstaclePrimaryConflictSeen = false;
    bool adjustedTargetSeen = false;
    bool conflictHoldSeen = false;
    bool executionSeen = false;
    bool nonZeroExecutedDemandSeen = false;
    bool lateralExecutedDemandSeen = false;
    bool passedObstaclePlane = false;
    bool reachedGoal = false;

    double initialGoalDistanceMeters = 0.0;
    double minimumGoalDistanceMeters = 0.0;
    double maximumStraightLineDeviationMeters = 0.0;

    double minimumObstacleCenterDistanceMeters = 0.0;
    double minimumConservativeClearanceMeters = 0.0;

    double maximumExecutedLinearDemandMps2 = 0.0;
    double maximumExecutedLateralDemandMps2 = 0.0;

    double maximumAppliedEngineAccelerationMps2 = 0.0;
    double maximumAppliedLateralAccelerationMps2 = 0.0;
    double maximumRelativeSpeedMps = 0.0;

    glm::dvec3 lastExecutedLinearDemandMapMps2 {0.0};
};

} // namespace game::diagnostics
