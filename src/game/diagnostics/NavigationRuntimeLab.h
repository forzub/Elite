#pragma once

#include <cstddef>
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
// The proving obstacle is authored first so the start line cannot silently
// drift away from it when the NAV STRESS layout is edited.
inline const glm::dvec3 NavigationRuntimeLabObstacleVisualLocalMeters {
    975.0,
    -1300.0,
    -4900.0
};

// Keep the obstacle inside the very first bounded local horizon. At zero
// relative speed the live policy starts at roughly turnDistance(1400 m) plus
// safety margin, so 1300 m forces exact-static participation before natural
// rotating-frame drift can carry the ship around the OBB.
inline constexpr double NavigationRuntimeLabInitialObstacleLeadMeters =
    1300.0;

// This start/goal pair therefore corresponds to visual:
//   start = { 975, -1300, -6200 }
//   obstacle = { 975, -1300, -4900 }
//   goal  = { 975, -1300,  1000 }
inline const glm::dvec3 NavigationRuntimeLabStartVisualLocalMeters {
    NavigationRuntimeLabObstacleVisualLocalMeters.x,
    NavigationRuntimeLabObstacleVisualLocalMeters.y,
    NavigationRuntimeLabObstacleVisualLocalMeters.z -
        NavigationRuntimeLabInitialObstacleLeadMeters
};

inline const glm::dvec3 NavigationRuntimeLabGoalVisualLocalMeters {
    NavigationRuntimeLabObstacleVisualLocalMeters.x,
    NavigationRuntimeLabObstacleVisualLocalMeters.y,
    1000.0
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

    // Legacy names retained as diagnostics. Under Stage 12A-5 the
    // static proving obstacle must remain false in both dynamic fields.
    bool obstacleCandidateSeen = false;
    bool obstaclePrimaryConflictSeen = false;
    bool obstacleExactStaticBlockSeen = false;

    std::uint64_t dynamicQueryCount = 0;
    std::size_t maximumDynamicCandidateCount = 0;

    bool adjustedTargetSeen = false;
    bool conflictHoldSeen = false;
    bool executionSeen = false;
    bool nonZeroExecutedDemandSeen = false;
    bool lateralExecutedDemandSeen = false;
    bool passedObstaclePlane = false;
    bool reachedGoal = false;

    bool exactStaticGeometryPublished = false;
    std::size_t exactStaticObstacleCount = 0;

    // Publication-time fixture proof: the configured centerline itself must
    // intersect the exact HitVolume of CUBE 08 before any flight begins.
    bool configuredRouteExactObstacleBlockPublished = false;

    // First live-plan geometry probe. This uses the exact same current
    // agent position, bounded horizon target and ship envelope that the
    // runtime planner should prove statically.
    bool placementProbeCaptured = false;
    glm::dvec3 placementPositionMap {0.0};
    glm::dvec3 placementMotionLocalTactical {0.0};
    double placementServerTimeSeconds = 0.0;

    bool firstLiveNominalProbeCaptured = false;
    bool firstLiveNominalExactBlocked = false;
    std::uint32_t firstLiveNominalBlockingEntityId = 0;
    double firstLiveHorizonMeters = 0.0;
    glm::dvec3 firstLiveAgentPositionMap {0.0};
    glm::dvec3 firstLiveGoalPositionMap {0.0};
    glm::dvec3 firstLiveBoundedTargetMap {0.0};
    double firstLiveServerTimeSeconds = 0.0;

    bool exactStaticQuerySeen = false;
    bool nominalStaticBlockSeen = false;
    std::size_t maximumExactStaticObstaclesExamined = 0;

    // Actual authoritative motion is sampled as swept segments against the
    // exact static layer. Conservative sphere clearance remains diagnostic only.
    bool exactStaticViolationSeen = false;
    std::uint64_t exactStaticMotionSamples = 0;

    bool firstExactStaticViolationCaptured = false;
    std::uint32_t firstExactStaticViolationEntityId = 0;
    glm::dvec3 firstExactStaticViolationStartMap {0.0};
    glm::dvec3 firstExactStaticViolationEndMap {0.0};
    glm::dvec3 firstExactStaticViolationSelectedTargetMap {0.0};
    std::uint8_t firstExactStaticViolationPlannerStatus = 0;

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
