#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>

#include "src/game/navigation/VehicleDynamicsProfile.h"
#include "src/world/navigation/control/PilotSkillExecutor.h"

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

    // Negative means "use the authored scenario value". The runtime viewer
    // uses explicit overrides so start/finish speed can be swept without
    // rewriting scenario.json. Speed is a route-planning input because higher
    // inertia requires more geometric maneuver room.
    double startSpeedOverrideMps = -1.0;
    double finishSpeedOverrideMps = -1.0;
};

using ScenarioVehicleParameters =
    game::navigation::VehicleDynamicsProfile;

[[nodiscard]] ScenarioVehicleParameters makeScenarioVehicleParameters(
    const ShipDescriptor& descriptor,
    std::uint64_t capabilityRevision = 1
);

struct ScenarioRunResult
{
    bool success = false;
    std::string message;
    TraceDocument trace;
    std::vector<std::string> diagnostics;

    // Authored defaults exposed to the viewer so its sliders initialize from
    // scenario data rather than hard-coded UI values.
    double authoredStartSpeedMps = 0.0;
    double authoredFinishSpeedMps = 0.0;
};

// Load only the authored input scene for pre-calculation visualization.
[[nodiscard]] ScenarioRunResult loadScenarioPreview(
    const std::string& scenarioJsonPath,
    const ScenarioVehicleParameters& vehicle
);

[[nodiscard]] ScenarioRunResult calculateScenario(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings,
    const ScenarioVehicleParameters& vehicle
);

[[nodiscard]] ScenarioRunResult executeCalculatedRoute(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings,
    const TraceDocument& calculatedRoute,
    const ScenarioVehicleParameters& vehicle
);

} // namespace elite::tools::navigation_runtime
