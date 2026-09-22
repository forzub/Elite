#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>

#include "src/game/navigation/VehicleDynamicsProfile.h"
#include "src/world/navigation/control/PilotSkillExecutor.h"
#include "src/world/navigation/TrajectoryGenerator.h"
#include "src/world/navigation/NavigationObstacle.h"

class ShipDescriptor;

#include "NavigationTrace.h"

namespace elite::tools::navigation_runtime
{

enum class ControlMode
{
    Assisted = 0,
    Newtonian
};

enum class PilotLevel
{
    Expert = 0,
    Average,
    Loser
};

enum class FlightStyle
{
    Standard = 0,
    Extreme
};

using ScenarioPilotSkillProfile =
    world::navigation::PilotSkillExecutor::PilotSkillProfile;

[[nodiscard]] ScenarioPilotSkillProfile makeScenarioPilotSkillProfile(
    PilotLevel level
) noexcept;

struct ScenarioNavigationPolicy
{
    // Runtime clock / sampling. These values affect integration and therefore
    // are explicit inputs, not file-scope constants.
    double executionDtSeconds = 1.0 / 120.0;
    double traceSampleSeconds = 1.0 / 30.0;
    double maximumExecutionOverrunSeconds = 30.0;

    // Static-route maneuver reserve policy.
    double representativeTurnAngleRad =
        3.14159265358979323846 / 6.0;
    double standardClearanceReserveFactor = 1.0;
    double extremeClearanceReserveFactor = 0.35;

    // Stage-1 geometric search doctrine.
    double geometricSupportMarginMeters = 2.0;
    double geometricMinimumSupportMarginMeters = 0.25;
    double geometricSupportMarginObstacleRadiusFactor = 0.03;
    int geometricSphereRadialSamples = 16;
    int geometricCapsuleRadialSamples = 12;
    std::size_t geometricMaxConsideredObstacles = 0;
    bool geometricAllowStartEscape = false;
    bool geometricAllowGoalEscape = false;
    bool geometricSimplifyLineOfSight = true;

    // Physical attitude / propulsion authoring policy.
    double lowSpeedDirectionThresholdMps = 0.25;
    double newtonianRcsPrimaryThresholdMps2 = 0.35;
    double terminalOrientationBlendDistanceMeters = 35.0;

    // Accepted-program terminal tolerances.
    double programTerminalPositionToleranceMeters = 4.0;
    double programTerminalSpeedToleranceMps = 2.0;
    double programTerminalForwardToleranceRad = 0.20;
    double programTerminalAngularVelocityToleranceRadPerSec = 0.50;
    double programValidityGraceSeconds = 5.0;

    // Tracking envelope / reserve.
    double trackingPositionErrorMeters = 8.0;
    double trackingLinearVelocityErrorMps = 4.0;
    double trackingForwardAngleErrorRad = 0.35;
    double trackingAngularVelocityErrorRadPerSec = 0.8;
    double alongTrackPositionDeadbandMeters = 12.0;
    double alongTrackSpeedDeadbandMps = 0.5;
    double linearFeedbackReserveMps2 = 1.5;
    double angularFeedbackReserveRadPerSec2 = 0.8;

    // B10 controller gains. They are execution policy, not hidden controller
    // defaults, so the caller owns them explicitly.
    double trackingPositionGainPerSecond2 = 0.50;
    double trackingVelocityGainPerSecond = 1.00;
    double trackingAttitudeGainPerSecond2 = 2.00;
    double trackingAngularVelocityGainPerSecond = 3.00;

    // A FreeTransit program that is outside its proved envelope for longer
    // than this is invalidated; Autopilot must not home indefinitely to stale
    // reference data.
    double trackingLossInvalidateSeconds = 0.50;
    double finalCaptureOverrunSeconds = 6.0;

    // Final physical-state acceptance for this diagnostic scenario runner.
    double finalPositionToleranceMeters = 5.0;
    double finalSpeedToleranceMps = 1.5;
    double finalForwardToleranceRad = 0.25;
    double finalUpToleranceRad = 0.25;

    [[nodiscard]] bool valid() const noexcept
    {
        const auto finiteNonNegative = [](double v) noexcept
        {
            return std::isfinite(v) && v >= 0.0;
        };

        return
            std::isfinite(executionDtSeconds) &&
            executionDtSeconds > 0.0 &&
            std::isfinite(traceSampleSeconds) &&
            traceSampleSeconds > 0.0 &&
            finiteNonNegative(maximumExecutionOverrunSeconds) &&
            finiteNonNegative(representativeTurnAngleRad) &&
            finiteNonNegative(standardClearanceReserveFactor) &&
            finiteNonNegative(extremeClearanceReserveFactor) &&
            finiteNonNegative(geometricSupportMarginMeters) &&
            finiteNonNegative(geometricMinimumSupportMarginMeters) &&
            finiteNonNegative(geometricSupportMarginObstacleRadiusFactor) &&
            geometricSphereRadialSamples >= 3 &&
            geometricCapsuleRadialSamples >= 3 &&
            finiteNonNegative(lowSpeedDirectionThresholdMps) &&
            finiteNonNegative(newtonianRcsPrimaryThresholdMps2) &&
            std::isfinite(terminalOrientationBlendDistanceMeters) &&
            terminalOrientationBlendDistanceMeters > 0.0 &&
            finiteNonNegative(programTerminalPositionToleranceMeters) &&
            finiteNonNegative(programTerminalSpeedToleranceMps) &&
            finiteNonNegative(programTerminalForwardToleranceRad) &&
            finiteNonNegative(programTerminalAngularVelocityToleranceRadPerSec) &&
            finiteNonNegative(programValidityGraceSeconds) &&
            finiteNonNegative(trackingPositionErrorMeters) &&
            finiteNonNegative(trackingLinearVelocityErrorMps) &&
            finiteNonNegative(trackingForwardAngleErrorRad) &&
            finiteNonNegative(trackingAngularVelocityErrorRadPerSec) &&
            finiteNonNegative(alongTrackPositionDeadbandMeters) &&
            finiteNonNegative(alongTrackSpeedDeadbandMps) &&
            finiteNonNegative(linearFeedbackReserveMps2) &&
            finiteNonNegative(angularFeedbackReserveRadPerSec2) &&
            finiteNonNegative(trackingPositionGainPerSecond2) &&
            finiteNonNegative(trackingVelocityGainPerSecond) &&
            finiteNonNegative(trackingAttitudeGainPerSecond2) &&
            finiteNonNegative(trackingAngularVelocityGainPerSecond) &&
            finiteNonNegative(trackingLossInvalidateSeconds) &&
            finiteNonNegative(finalCaptureOverrunSeconds) &&
            finiteNonNegative(finalPositionToleranceMeters) &&
            finiteNonNegative(finalSpeedToleranceMps) &&
            finiteNonNegative(finalForwardToleranceRad) &&
            finiteNonNegative(finalUpToleranceRad);
    }
};

struct ScenarioRuntimeIoPolicy
{
    // Empty directory disables file output. The runtime never resolves the
    // process working directory by itself; an orchestration boundary must pass
    // the destination explicitly.
    std::string diagnosticsDirectory;
    bool writeRouteDiagnostics = false;
    bool writeExecutionDiagnostics = false;
    bool writeExecutionTelemetry = false;
    bool echoDiagnosticsToConsole = false;
};

struct ScenarioRunSettings
{
    ControlMode controlMode = ControlMode::Newtonian;
    PilotLevel pilot = PilotLevel::Expert; // presentation label only
    ScenarioPilotSkillProfile pilotExecutionProfile =
        makeScenarioPilotSkillProfile(PilotLevel::Expert);
    FlightStyle flightStyle = FlightStyle::Standard;
    bool enableSuddenObstacle = false;

    // All calculation-affecting stand policy crosses the API explicitly.
    ScenarioNavigationPolicy navigation {};

    // Canonical route->trajectory backend policy. Keeping the exact backend
    // policy object at this API boundary prevents TrajectoryGenerator from
    // silently falling back to its struct defaults when the runtime forgets
    // to populate a field.
    world::navigation::TrajectoryGenerationPolicy trajectory {};

    // Side effects are also explicit. They are orchestration output policy and
    // do not participate in navigation calculations.
    ScenarioRuntimeIoPolicy io {};

    // Negative means "use the authored scenario value". The runtime viewer
    // uses explicit overrides so start/finish speed can be swept without
    // rewriting scenario.json. Speed is a route-planning input because higher
    // inertia requires more geometric maneuver room.
    double startSpeedOverrideMps = -1.0;
    double finishSpeedOverrideMps = -1.0;
};

struct ScenarioBasis
{
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 right {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

struct ScenarioEndpoint
{
    glm::dvec3 position {300.0, 0.0, 0.0};
    bool requireForward = false;
    bool requireUp = false;
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    double speedMps = 0.0;
};

struct ScenarioMotionPath
{
    std::vector<glm::dvec3> points;
    double speedMps = 0.0;
    bool loop = false;
};

struct ScenarioDynamicObstacleDefinition
{
    std::uint64_t entityId = 0;
    std::string id;
    glm::dvec3 initialPosition {0.0};
    glm::dvec3 linearVelocity {0.0};
    ScenarioMotionPath path {};
    double activationTimeSeconds = 0.0;
    double radiusMeters = 5.0;
    bool spawnRelativeToShip = false;
    glm::dvec3 spawnRelativeFruMeters {0.0};
};

// Immutable parsed scenario snapshot. File I/O ends at loadScenarioDefinition;
// all planning/execution functions consume this value and never reopen the
// source file across the Stage-1/Stage-2 boundary.
struct ScenarioDefinition
{
    std::uint64_t goalRevision = 1;
    std::uint64_t staticWorldRevision = 1;
    std::uint64_t dynamicWorldRevision = 1;

    glm::dvec3 startPosition {0.0};
    glm::dvec3 startVelocity {6.0, 0.0, 0.0};
    glm::dvec3 startAcceleration {0.0};
    double startPitchRateRadPerSec = 0.0;
    double startYawRateRadPerSec = 0.0;
    double startRollRateRadPerSec = 0.0;
    ScenarioBasis startBasis {};

    std::vector<glm::dvec3> shipRoutePoints;
    ScenarioEndpoint finish {};

    std::vector<world::navigation::NavigationObstacle> staticObstacles;
    std::vector<ScenarioDynamicObstacleDefinition> dynamicObstacles;
    ScenarioDynamicObstacleDefinition suddenObstacle {};
    bool hasSuddenObstacle = false;

    double routeClearanceMeters = 0.0;
};

[[nodiscard]] ScenarioDefinition loadScenarioDefinition(
    const std::string& scenarioJsonPath
);

using ScenarioVehicleParameters =
    game::navigation::VehicleDynamicsProfile;

[[nodiscard]] ScenarioVehicleParameters makeScenarioVehicleParameters(
    const ShipDescriptor& descriptor,
    std::uint64_t capabilityRevision = 1
);

struct RetainedStaticRoute
{
    bool valid = false;
    std::uint64_t goalRevision = 0;
    std::uint64_t staticWorldRevision = 0;
    std::uint64_t vehicleCapabilityRevision = 0;

    double planningSpeedMps = 0.0;
    double additionalClearanceMeters = 0.0;
    std::vector<glm::dvec3> pointsMapMeters;
};

struct ScenarioRunResult
{
    bool success = false;
    std::string message;
    TraceDocument trace;
    RetainedStaticRoute retainedRoute;
    std::vector<std::string> diagnostics;

    // Authored defaults exposed to the viewer so its sliders initialize from
    // scenario data rather than hard-coded UI values.
    double authoredStartSpeedMps = 0.0;
    double authoredFinishSpeedMps = 0.0;
};

// Load only the authored input scene for pre-calculation visualization.
[[nodiscard]] ScenarioRunResult loadScenarioPreview(
    const ScenarioDefinition& scenario,
    const ScenarioVehicleParameters& vehicle
);

[[nodiscard]] ScenarioRunResult calculateScenario(
    const ScenarioDefinition& scenario,
    const ScenarioRunSettings& settings,
    const ScenarioVehicleParameters& vehicle
);

[[nodiscard]] ScenarioRunResult executeCalculatedRoute(
    const ScenarioDefinition& scenario,
    const ScenarioRunSettings& settings,
    const RetainedStaticRoute& retainedRoute,
    const ScenarioVehicleParameters& vehicle
);

} // namespace elite::tools::navigation_runtime
