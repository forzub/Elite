#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace elite::tools::navigation_runtime
{

struct TraceFrame
{
    double timeSeconds = 0.0;

    glm::dvec3 shipPosition {0.0};
    glm::dvec3 shipForward {1.0, 0.0, 0.0};
    glm::dvec3 shipRight {0.0, 0.0, 1.0};
    glm::dvec3 shipUp {0.0, 1.0, 0.0};
    glm::dvec3 shipVelocity {0.0};

    // Physical angular/propulsion telemetry. This is recorded per trace frame
    // so a visible hull flip can be correlated with actual engine activity
    // instead of inferred from animation alone.
    glm::dvec3 shipAngularRatePyrRadPerSec {0.0};
    glm::dvec3 mainEngineAccelerationMps2 {0.0};
    glm::dvec3 manoeuvreAccelerationMps2 {0.0};
    glm::dvec3 engineAccelerationMps2 {0.0};

    // Control-chain telemetry: what Follower requested and what the selected
    // pilot profile actually delivered to SharedShipPhysics this tick.
    glm::dvec3 idealLinearAccelerationDemandMps2 {0.0};
    glm::dvec3 idealAngularAccelerationDemandRadPerSec2 {0.0};
    glm::dvec3 executedLinearAccelerationDemandMps2 {0.0};
    glm::dvec3 executedAngularAccelerationDemandRadPerSec2 {0.0};

    // Effective runtime state, not merely what the UI requested. The viewer
    // uses this to keep controls synchronized with the system actually flown.
    bool hasRuntimeControlLaw = false;
    std::string runtimeControlLaw;
    double mainEngineThrottle01 = 0.0;

    // Reference state sampled from the actually accepted maneuver program.
    // This is distinct from actual rigid-body attitude and lets the viewer
    // expose tracking/orientation mismatch instead of reconstructing attitude.
    bool hasProgramReference = false;
    glm::dvec3 programReferencePosition {0.0};
    glm::dvec3 programReferenceVelocity {0.0};
    glm::dvec3 programReferenceForward {1.0, 0.0, 0.0};
    glm::dvec3 programReferenceRight {0.0, 0.0, 1.0};
    glm::dvec3 programReferenceUp {0.0, 1.0, 0.0};
    double programTrackingCorridorRadiusMeters = 0.0;
    double programSpeedCorridorHalfWidthMps = 0.0;
    double programProgressCorridorHalfWidthMeters = 0.0;

    // Diagnostics for the currently active accepted program. The instantaneous
    // reference is the point the follower is tracking NOW; the phase target is
    // the end of the active program slice cut from the Ruckig-derived
    // trajectory. Neither is mislabeled as Ruckig's global scalar target.
    bool hasProgramPhaseTarget = false;
    glm::dvec3 programPhaseTargetPosition {0.0};

    // Planner-owned actuator command sampled from the accepted maneuver
    // interval. Kept separate from actual physical engine telemetry.
    bool hasPlannedActuatorCommand = false;
    std::size_t plannedActuatorSegmentIndex = 0;
    double plannedRearMainThrottle01 = 0.0;
    double plannedForeMainThrottle01 = 0.0;
    glm::dvec3 plannedManoeuvreAccelerationMps2 {0.0};
    bool plannedPropulsionFeasible = true;

    bool hazardActive = false;
    glm::dvec3 hazardPosition {0.0};
    glm::dvec3 hazardVelocity {0.0};
    double plannerLookAheadSeconds = 0.0;
    double hazardRadiusMeters = 0.0;
    double hazardCollisionEnvelopeRadiusMeters = 0.0;
    double hazardPlannerEnvelopeRadiusMeters = 0.0;
    double dynamicClearanceMeters = 0.0;

    std::string phase;
    std::string plannerStatus;
    bool replanEvent = false;

    bool hasSelectedTarget = false;
    glm::dvec3 selectedTarget {0.0};

    bool hasReacquisitionTarget = false;
    glm::dvec3 reacquisitionTarget {0.0};

    bool hasPortalTarget = false;
    glm::dvec3 portalTarget {0.0};
};

struct TraceStaticObstacle
{
    std::string id;
    std::string shape = "sphere";
    glm::dvec3 center {0.0};
    glm::dvec3 halfExtents {1.0};
    double radiusMeters = 1.0;
    double capsuleHalfLengthMeters = 0.0;
};

// Read-only presentation product for the B4/B5 physical-search seam.  These
// values are diagnostics, never an AcceptedManeuverProgram and never input to
// ship execution.
struct TracePhysicalSearchAlternative
{
    std::size_t index = 0;
    std::uint64_t corridorAlternativeId = 0;
    std::uint64_t terminalAlternativeId = 0;
    std::uint64_t speedScheduleAlternativeId = 0;
    std::uint64_t arrivalTimeAlternativeId = 0;
    glm::dvec3 targetPositionMapMeters {0.0};
    glm::dvec3 desiredVelocityMapMps {0.0};
    double maximumProgramSeconds = 0.0;
    bool attempted = false;
    bool selectedAlternative = false;
    std::string compilerStatus = "not_attempted";
    std::string infeasibilityReason = "none";
    double minimumProgramSeconds = 0.0;
};

struct TracePhysicalCandidateSample
{
    double timeOffsetSeconds = 0.0;
    glm::dvec3 positionMapMeters {0.0};
    glm::dvec3 velocityMapMps {0.0};
    glm::dvec3 accelerationMapMps2 {0.0};
    glm::dvec3 forwardMap {1.0, 0.0, 0.0};
};

struct TracePhysicalCandidate
{
    std::size_t alternativeIndex = 0;
    std::string family;
    bool requiresContinuousProof = true;
    std::vector<TracePhysicalCandidateSample> samples;
};

struct TracePhysicalSearch
{
    bool available = false;
    std::string coordinatorStatus = "not_run";
    std::uint64_t objectiveRevision = 0;
    std::uint64_t frontierRevision = 0;
    bool objectiveRemainsActive = true;
    std::vector<TracePhysicalSearchAlternative> alternatives;
    std::vector<TracePhysicalCandidate> candidates;
};

struct TraceDocument
{
    int version = 2;
    std::string law;
    glm::dvec3 shipHalfExtentsMeters {1.0};

    // Authored scene endpoints exist before any planner result. The viewer
    // must be able to display/focus the scene before Calculate is pressed.
    bool hasSceneEndpoints = false;
    glm::dvec3 sceneStartMapMeters {0.0};
    glm::dvec3 sceneFinishMapMeters {0.0};

    std::vector<glm::dvec3> routePoints;
    std::vector<glm::dvec3> turnPoints;

    // Stage-2 diagnostics: the local guide generated from the retained coarse
    // route and the full collision-checked Ruckig reference trajectory.
    std::vector<glm::dvec3> executionGuidePoints;
    std::vector<glm::dvec3> calculatedTrajectoryPoints;

    // Observer-only B4/B5 result.  The viewer renders it separately from the
    // legacy Ruckig curve and from any accepted/executed reference.
    TracePhysicalSearch physicalSearch;

    std::vector<TraceStaticObstacle> staticObstacles;
    std::vector<TraceFrame> frames;
};

void saveTraceJson(
    const TraceDocument& trace,
    const std::string& path
);

TraceDocument loadTraceJson(const std::string& path);

} // namespace elite::tools::navigation_runtime
