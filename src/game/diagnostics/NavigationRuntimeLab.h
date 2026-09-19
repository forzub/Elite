#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

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
inline constexpr const char* NavigationRuntimeLabRotatingActorLabel =
    "GUIDANCE DOCK CUBE A";
inline const glm::dvec3 NavigationRuntimeLabRotatingActorAngularVelocityDegPerSecond {
    0.0,
    0.0,
    2.0
};
// StaticObject currently stores authoritative infrastructure angular velocity
// in glm::vec3. The authored diagnostic rate is computed in double, then
// quantized once through that float storage before crossing the typed
// System -> NavLocal boundary. The live acceptance tolerance therefore has to
// cover one float quantization at this magnitude; 1e-12 incorrectly tests
// double precision that the source state does not possess.
inline constexpr double NavigationRuntimeLabAngularVelocityToleranceRadPerSecond =
    1.0e-8;

// Stage 12A-6b3b deterministic live moving aperture. These are real
// hub-attached physical objects whose centres translate together in hub-visual
// coordinates. The moving gap is the first authority event in the route.
inline constexpr const char* NavigationRuntimeLabMovingGapUpperLabel =
    "NAV MOVING GAP UPPER";
inline constexpr const char* NavigationRuntimeLabMovingGapLowerLabel =
    "NAV MOVING GAP LOWER";
// GuidanceDockCube is 360 m high. Keep the two centres 500 m apart so
// the real HitVolume OBBs leave a 140 m physical aperture. The aperture is
// deliberately centred at Y=-900 while the authored route starts at Y=-1300:
// the direct route intersects the lower OBB, but a bounded upward visibility
// deflection can enter the real gap. This fixture therefore requires exact
// dynamic geometry; a conservative enclosing sphere is broadphase only.
inline constexpr double NavigationRuntimeLabMovingGapApertureCenterVisualY =
    -900.0;
inline constexpr double NavigationRuntimeLabMovingGapBoundarySeparationMeters =
    500.0;

inline const glm::dvec3 NavigationRuntimeLabMovingGapUpperVisualLocalMeters {
    975.0,
    NavigationRuntimeLabMovingGapApertureCenterVisualY +
        0.5 * NavigationRuntimeLabMovingGapBoundarySeparationMeters,
    -5700.0
};
inline const glm::dvec3 NavigationRuntimeLabMovingGapLowerVisualLocalMeters {
    975.0,
    NavigationRuntimeLabMovingGapApertureCenterVisualY -
        0.5 * NavigationRuntimeLabMovingGapBoundarySeparationMeters,
    -5700.0
};
inline const glm::dvec3 NavigationRuntimeLabMovingGapVelocityVisualMps {
    0.0, 0.0, 1.0
};
inline constexpr double NavigationRuntimeLabMovingPassageDurationSeconds =
    30.0;

// StaticObject::linearVelocity is currently glm::vec3. Orbital hub velocity is
// thousands of m/s, so converting the authoritative world velocity through
// that float field can leave millimetres-per-second of NavLocal residue.
// Keep the live kinematic proof tighter than gameplay significance but honest
// about the source representation.
inline constexpr double NavigationRuntimeLabLinearVelocityToleranceMps =
    1.0e-2;

inline bool isNavigationRuntimeLabMovingGapBoundary(
    std::string_view label
) noexcept
{
    return
        label == NavigationRuntimeLabMovingGapUpperLabel ||
        label == NavigationRuntimeLabMovingGapLowerLabel;
}

// Stage 12A-6b3b static slit/tunnel gate.
//
// Six 360 x 360 x 900 m exact-HitVolume cubes form two three-cube rows.
// The 140 m vertical gap between the rows is a real 900 m-deep tunnel.
// The portal center is intentionally 120 m above the authored straight route,
// so successful navigation must steer into the slit instead of merely flying
// straight through a pre-aligned hole.
inline const glm::dvec3 NavigationRuntimeLabSlitPortalCenterVisualLocalMeters {
    975.0, -1180.0, -4500.0
};
inline constexpr double NavigationRuntimeLabSlitHalfWidthMeters = 540.0;
inline constexpr double NavigationRuntimeLabSlitHalfHeightMeters = 70.0;
inline constexpr double NavigationRuntimeLabSlitHalfDepthMeters = 450.0;
inline constexpr double NavigationRuntimeLabSlitPortalClearanceMeters = 55.0;
inline constexpr std::uint64_t NavigationRuntimeLabSlitEntryPortalId = 1202301;
inline constexpr std::uint64_t NavigationRuntimeLabSlitExitPortalId = 1202302;

inline const glm::dvec3 NavigationRuntimeLabSlitEntryCenterVisualLocalMeters {
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x,
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y,
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.z -
        NavigationRuntimeLabSlitHalfDepthMeters
};
inline const glm::dvec3 NavigationRuntimeLabSlitExitCenterVisualLocalMeters {
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x,
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y,
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.z +
        NavigationRuntimeLabSlitHalfDepthMeters
};

// Generic authored passage-capture requirements for this test tunnel.
inline constexpr double NavigationRuntimeLabSlitApproachDistanceMeters = 500.0;
inline constexpr double NavigationRuntimeLabSlitTransitSpeedMps = 20.0;
inline constexpr double NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad =
    0.08726646259971647; // 5 degrees.
inline constexpr double NavigationRuntimeLabSlitMaximumEntryForwardAngleRad =
    0.08726646259971647; // 5 degrees.
inline constexpr double NavigationRuntimeLabSlitMaximumLateralSpeedMps = 1.0;

// CUBE 08 remains the representative exact-static blocker identity, but is now
// the centre cube in the lower tunnel row rather than an isolated obstacle.
inline const glm::dvec3 NavigationRuntimeLabObstacleVisualLocalMeters {
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x,
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y - 250.0,
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.z
};

// Hub ReferenceFrame uses tactical local axes:
//   X = prograde, Y = radial, Z = normal.
//
// Stress/tunnel objects are authored in visual hub axes:
//   X = normal, Y = radial, Z = -prograde.
//
// Keep the same deterministic start. Moving gap is ~500 m ahead; the static
// slit portal is ~1700 m ahead.
inline constexpr double NavigationRuntimeLabInitialObstacleLeadMeters =
    1700.0;

inline const glm::dvec3 NavigationRuntimeLabStartVisualLocalMeters {
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x,
    -1300.0,
    NavigationRuntimeLabObstacleVisualLocalMeters.z -
        NavigationRuntimeLabInitialObstacleLeadMeters
};

inline const glm::dvec3 NavigationRuntimeLabGoalVisualLocalMeters {
    NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x,
    -1300.0,
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

// World-position round trips around orbital-scale coordinates are performed
// with double precision. Sub-millimetre residue is numerical noise, not a
// navigation placement defect.
inline constexpr double NavigationRuntimeLabPlacementToleranceMeters = 1.0e-3;
inline constexpr double NavigationRuntimeLabWorkspaceHalfExtentMeters = 12000.0;

struct NavigationRuntimeLabObservation
{
    bool valid = false;

    std::uint32_t shipEntityId = 0;
    std::uint32_t obstacleEntityId = 0;

    std::uint32_t rotatingActorEntityId = 0;
    bool rotatingActorCandidateSeen = false;
    bool rotatingActorAngularVelocityVerified = false;
    glm::dvec3 expectedRotatingActorAngularVelocityMapRadPerSecond {0.0};
    glm::dvec3 observedRotatingActorAngularVelocityMapRadPerSecond {0.0};
    double rotatingActorAngularVelocityErrorRadPerSecond = 0.0;

    // Live moving-gap fixture identity/publication.
    std::uint32_t movingGapUpperEntityId = 0;
    std::uint32_t movingGapLowerEntityId = 0;
    bool movingGapPairCandidateSeen = false;
    bool movingGapKinematicsVerified = false;
    double movingGapMaximumVelocityErrorMps = 0.0;
    glm::dvec3 movingGapCurrentCenterMap {0.0};

    // Sticky proof flags plus current authority/execution state. The current
    // flags let the server replication gate require a sparse publication from
    // an epoch in which MovingPassageClear actually owned the ship.
    bool movingPrecisionAttemptedSeen = false;
    bool movingPassageFeasibleSeen = false;
    bool movingPassageStaticSafeSeen = false;
    bool movingPassageAuthoritySeen = false;
    bool movingPassageAuthorityActive = false;
    bool movingPassageExecutedSeen = false;
    bool movingPassageExecutionActive = false;
    bool movingPassageAppliedAccelerationSeen = false;

    // Default free-space transit evidence. The moving pair is an obstacle
    // encounter, not a mandatory gap: bounded visibility steering must choose
    // the smallest safe deflection, physically execute it, then return to the
    // direct accepted target as soon as that corridor becomes clear.
    bool visibilityBypassSeen = false;
    bool visibilityBypassActive = false;
    bool visibilityDirectRecoveredSeen = false;
    double maximumVisibilityDeflectionRad = 0.0;

    bool movingGapPlanePassed = false;

    bool slitPortalExactOpenPublished = false;
    std::size_t slitPortalExactObstaclesExamined = 0;
    bool slitPortalWaypointSeen = false;
    bool slitEntryCaptureSeen = false;
    bool slitEntryVelocityAlignedSeen = false;
    bool slitEntryForwardAlignedSeen = false;
    bool slitEntryPlaneCrossedAligned = false;
    double slitEntryVelocityAngleRad = 0.0;
    double slitEntryForwardAngleRad = 0.0;
    double slitEntryLateralSpeedMps = 0.0;
    double slitEntryCrossTrackMeters = 0.0;
    bool slitTunnelPassed = false;
    glm::dvec3 slitTunnelCrossingMap {0.0};
    double slitTunnelCrossingMarginMeters = 0.0;

    std::uint8_t movingPassageLastEvaluatorStatus = 0xffu;
    double movingPassageRequiredPeakForwardAccelerationMps2 = 0.0;
    double movingPassageRequiredPeakReverseAccelerationMps2 = 0.0;
    double movingPassageRequiredPeakLateralAccelerationMps2 = 0.0;
    double movingPassageRequiredPeakVerticalAccelerationMps2 = 0.0;
    double movingPassageMinimumSampleClearanceMeters = 0.0;
    double movingPassageMinimumContinuousClearanceMeters = 0.0;

    double maximumMovingPassageExecutedDemandMps2 = 0.0;
    double maximumMovingPassageAppliedAccelerationMps2 = 0.0;
    glm::dvec3 lastMovingPassageExecutedWorldMps2 {0.0};

    std::uint64_t planCount = 0;
    std::uint64_t executionCount = 0;
    std::uint64_t acceptedSegmentFollowCount = 0;
    std::uint64_t acceptedSegmentReplanCount = 0;
    std::uint64_t acceptedSegmentStaticSafetyInvalidationCount = 0;
    std::uint64_t acceptedSegmentEmergencyRecoveryCount = 0;
    bool acceptedSegmentEmergencyRecoveryActive = false;
    bool acceptedSegmentLastStoppingReserveBlocked = false;
    glm::dvec3 acceptedSegmentLastStoppingReserveEndMap {0.0};
    double acceptedSegmentLastStoppingReserveSeconds = 0.0;
    double acceptedSegmentLastStoppingReserveDistanceMeters = 0.0;
    std::uint32_t acceptedSegmentLastStaticBlockingEntityId = 0;
    bool acceptedSegmentLastStaticTargetBlocked = false;
    bool acceptedSegmentLastStaticForecastBlocked = false;
    bool acceptedSegmentLastStaticExecutedForecastBlocked = false;
    glm::dvec3 acceptedSegmentLastStaticProbeStartMap {0.0};
    glm::dvec3 acceptedSegmentLastStaticTargetMap {0.0};
    glm::dvec3 acceptedSegmentLastStaticForecastEndMap {0.0};
    glm::dvec3 acceptedSegmentLastStaticExecutedForecastEndMap {0.0};
    glm::dvec3 acceptedSegmentLastStaticVelocityMapMps {0.0};
    glm::dvec3 acceptedSegmentLastStaticIdealAccelerationMapMps2 {0.0};
    glm::dvec3 acceptedSegmentLastStaticExecutedAccelerationMapMps2 {0.0};
    double acceptedSegmentLastStaticForecastSeconds = 0.0;
    double acceptedSegmentLastStaticProbeTimeSeconds = 0.0;
    std::uint64_t acceptedSegmentRevision = 0;
    std::uint8_t lastReplanReason = 0xffu;
    bool acceptedSegmentActive = false;
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
    std::uint64_t firstExactStaticViolationAcceptedSegmentRevision = 0;
    std::uint8_t firstExactStaticViolationLastReplanReason = 0xffu;
    glm::dvec3 firstExactStaticViolationAcceptedTargetMap {0.0};
    glm::dvec3 firstExactStaticViolationAcceptedTargetVelocityMapMps {0.0};
    glm::dvec3 firstExactStaticViolationCurrentVelocityMapMps {0.0};
    glm::dvec3 firstExactStaticViolationLastExecutedDemandMapMps2 {0.0};
    bool firstExactStaticViolationPreviousTargetBlocked = false;
    bool firstExactStaticViolationPreviousForecastBlocked = false;
    bool firstExactStaticViolationPreviousExecutedForecastBlocked = false;
    std::uint32_t firstExactStaticViolationPreviousBlockingEntityId = 0;
    glm::dvec3 firstExactStaticViolationPreviousProbeStartMap {0.0};
    glm::dvec3 firstExactStaticViolationPreviousTargetMap {0.0};
    glm::dvec3 firstExactStaticViolationPreviousForecastEndMap {0.0};
    glm::dvec3 firstExactStaticViolationPreviousExecutedForecastEndMap {0.0};
    glm::dvec3 firstExactStaticViolationPreviousVelocityMapMps {0.0};
    glm::dvec3 firstExactStaticViolationPreviousIdealAccelerationMapMps2 {0.0};
    glm::dvec3 firstExactStaticViolationPreviousExecutedAccelerationMapMps2 {0.0};
    double firstExactStaticViolationPreviousForecastSeconds = 0.0;
    double firstExactStaticViolationPreviousProbeTimeSeconds = 0.0;

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
