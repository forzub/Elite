#pragma once

/*
    Template-independent half of the route autopilot
    (DedicatedClientRouteAutopilot): the continuous spatial reference built
    from the accepted programs, the runtime speed profile (checkpoints,
    braking feasibility, thrust-reversal coast, capability reprofile), the
    route curve <-> sample progress maps and the accepted-program builder.
    It works on RouteExecutionState only; the pilot controller and the
    per-law control flow stay in the template.
*/

#include "src/game/navigation/autopilot/RouteExecutionState.h"

namespace game::navigation::autopilot
{

class RouteExecutionCore
{
protected:
    using State = RouteExecutionState;

    struct ContinuousReferenceDiagnostic
    {
        bool valid = false;
        AcceptedManeuverProgram::ReferenceSample reference {};
        std::size_t lowerSampleIndex = 0;
        std::size_t upperSampleIndex = 0;
        double interpolation01 = 0.0;
        double spatialProgressMeters = 0.0;
    };

    [[nodiscard]] static bool buildContinuousReference(
        const std::vector<AcceptedManeuverProgram>& programs,
        std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        std::vector<double>& progress
    );

    [[nodiscard]] static ContinuousReferenceDiagnostic
    sampleContinuousReference(
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        const glm::dvec3& positionMapMeters,
        std::size_t minimumSegmentIndex
    ) noexcept;

    [[nodiscard]] static bool initializeRuntimeProfile(
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        std::vector<double>& nominalSpeeds,
        std::vector<double>& runtimeSpeeds,
        std::vector<double>& runtimeAccelerations
    );

    [[nodiscard]] static bool buildCheckpointProgress(
        const std::vector<planner::RouteGate>& gates,
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        std::vector<double>& checkpoints
    );

    // Newtonian speed tolerance corridor at a checkpoint. On a speed
    // PLATEAU of the profile the craft is flying (cruise, curve, narrow-zone

    // The reference's own path-normal acceleration (v^2 * curvature) at a
    // sample, and its speed.
    [[nodiscard]] static double referenceNormalAccelerationMps2(
        const AcceptedManeuverProgram::ReferenceSample& sample,
        double& speedMps) noexcept;

    // Narrow zones ahead (and their aligned approach) at `share` of their
    // nominal speed.
    static void limitNarrowSpeedsToRcs(
        State& state, double progressMeters, double share) noexcept;

    // Lower the nominal speed ahead wherever the reference's path-normal
    // acceleration exceeds the planned lateral authority:
    // v_new = v * sqrt(a_lateral / a_normal).
    static void limitCurveSpeedsToLateral(State& state, double lateralMps2) noexcept;

    // Every speed restriction ahead is still physically reachable from the
    // actual speed: braking with the full authority after the hull turned
    // its thrust axis (lead) to the nominal speeds (zones, the route end),
    // and on curves at most the speed the FULL lateral authority holds on
    // the curve (the planned curve speed is a reserve, not the limit).
    [[nodiscard]] static bool canBrakeToSpeedsAhead(
        const State& state,
        double progressMeters,
        double speedMps,
        double brakingMps2,
        double leadSeconds,
        double fullLateralMps2
    ) noexcept;

    static void recomputeRuntimeSpeedSuffix(
        const std::vector<double>& nominalSpeeds,
        const std::vector<double>& progress,
        std::size_t anchorIndex,
        double actualSpeedMps,
        double acceleratingMps2,
        double brakingMps2,
        std::vector<double>& runtimeSpeeds,
        std::vector<double>& runtimeAccelerations,
        double thrustReversalCoastSeconds = 0.0,
        std::size_t toleratedPlateauEndIndex = 0
    ) noexcept;

    // DEV diagnostic only: ELITE_NEWTONIAN_PROFILE_TRACE=<path>.
    [[nodiscard]] static std::FILE* profileTraceFile() noexcept;

    // DEV diagnostic only: ELITE_ROUTE_DUMP=<path> appends every accepted
    // route: curves, then the runtime speed profile every ~50 m.
    static void dumpAcceptedRoute(
        const State& state,
        const planner::RoutePlan& plan
    ) noexcept;

    // A capability change is rare: always reported (stderr, the game log).
    static void traceCapabilityChange(
        const State& state,
        double universeTimeSeconds,
        int kind,
        double actualSpeedMps,
        const NavigationControlCapability& live,
        const char* action
    ) noexcept;

    static void traceProfile(
        const State& state,
        double universeTimeSeconds,
        double actualSpeedMps
    ) noexcept;

    // Acceleration -> coast -> braking. A pilot that must turn its thrust
    // axis before braking needs to reach every braking curve already
    // coasting for coastSeconds. At each sample find the highest speed from
    // which a coast of v*coastSeconds still ends on the braking-feasible
    // profile, then stop ACCELERATING once that speed is reached; the speed
    // is held (coast) until the unchanged braking curve takes over.
    static void insertThrustReversalCoast(
        const std::vector<double>& progress,
        std::size_t anchorIndex,
        double anchorSpeedMps,
        double acceleratingMps2,
        double coastSeconds,
        std::vector<double>& speeds
    ) noexcept;

    static void applyRuntimeProfile(
        const std::vector<double>& runtimeSpeeds,
        const std::vector<double>& runtimeAccelerations,
        LocalFlightControlLaw law,
        ContinuousReferenceDiagnostic& reference
    ) noexcept;

    [[nodiscard]] static glm::dvec3 rotateAroundAxis(
        const glm::dvec3& value,
        const glm::dvec3& axis,
        double angleRad
    ) noexcept;

    [[nodiscard]] static bool buildCurveSampleProgressMap(
        const std::vector<planner::RouteCurveSegment>& curves,
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        std::vector<double>& starts,
        std::vector<double>& ends
    );

    [[nodiscard]] static double mapSampleProgressToRouteProgress(
        const std::vector<planner::RouteCurveSegment>& curves,
        const std::vector<double>& starts,
        const std::vector<double>& ends,
        double sampleProgressMeters
    ) noexcept;

    [[nodiscard]] static double mapRouteProgressToSampleProgress(
        const std::vector<planner::RouteCurveSegment>& curves,
        const std::vector<double>& starts,
        const std::vector<double>& ends,
        double routeProgressMeters
    ) noexcept;

    struct RouteCurveDiagnostic
    {
        bool valid = false;
        std::size_t curveIndex = 0;
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 tangentMap {0.0, 0.0, -1.0};
        glm::dvec3 turnNormalMap {0.0};
        double curvaturePerMeter = 0.0;
        double maxSpeedMps = 0.0;
    };

    [[nodiscard]] static std::vector<planner::RouteCurveSegment>
    buildFallbackRouteCurves(
        const std::vector<planner::RouteGate>& gates
    );


    [[nodiscard]] static RouteCurveDiagnostic sampleRouteCurveAtProgress(
        const std::vector<planner::RouteCurveSegment>& curves,
        double routeProgressMeters
    ) noexcept;

    [[nodiscard]] static ContinuousReferenceDiagnostic
    sampleContinuousReferenceAtProgress(
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        double spatialProgressMeters
    ) noexcept;

    [[nodiscard]] static double angleBetween(
        const glm::dvec3& a,
        const glm::dvec3& b
    ) noexcept;

    static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept;

    static void makeBasis(
        const glm::dvec3& requestedForward,
        glm::dvec3& forward,
        glm::dvec3& right,
        glm::dvec3& up
    ) noexcept;

    [[nodiscard]] static std::vector<AcceptedManeuverProgram>
    buildPrograms(
        const planner::RoutePlan& plan,
        const AutopilotAgentState& initialAgent,
        LocalFlightControlLaw law,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        std::uint64_t requestSerial,
        double trackingPositionToleranceMeters,
        const glm::dvec3& routeUpReference,
        std::string* failureReason
    );
};

} // namespace game::navigation::autopilot
