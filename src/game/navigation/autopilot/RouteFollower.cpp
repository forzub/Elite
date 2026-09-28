#include "src/game/navigation/autopilot/RouteFollowerApi.h"

#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/TrajectoryFollower.h"

namespace game::navigation::autopilot
{

RouteFollowerResult RouteFollower::follow(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds,
    const RouteFollowerAgentState& agent,
    const RouteFollowerPolicy& policy,
    std::size_t minimumSpatialSegmentIndex
) noexcept
{
    game::navigation::TrajectoryFollower::AgentState legacyAgent;
    legacyAgent.positionMapMeters = agent.positionMapMeters;
    legacyAgent.velocityMapMetersPerSecond =
        agent.velocityMapMetersPerSecond;
    legacyAgent.forwardMap = agent.forwardMap;
    legacyAgent.rightMap = agent.rightMap;
    legacyAgent.upMap = agent.upMap;
    legacyAgent.pitchRateRadPerSec = agent.pitchRateRadPerSec;
    legacyAgent.yawRateRadPerSec = agent.yawRateRadPerSec;
    legacyAgent.rollRateRadPerSec = agent.rollRateRadPerSec;

    game::navigation::ManeuverTrackingController::Policy legacyPolicy;
    legacyPolicy.positionGainPerSecond2 = policy.positionGainPerSecond2;
    legacyPolicy.velocityGainPerSecond = policy.velocityGainPerSecond;
    legacyPolicy.attitudeGainPerSecond2 = policy.attitudeGainPerSecond2;
    legacyPolicy.angularVelocityGainPerSecond =
        policy.angularVelocityGainPerSecond;

    const auto legacy = game::navigation::TrajectoryFollower::follow(
        program,
        universeTimeSeconds,
        legacyAgent,
        legacyPolicy,
        minimumSpatialSegmentIndex
    );

    RouteFollowerResult out;
    switch (legacy.status)
    {
        case game::navigation::TrajectoryFollower::Status::Following:
            out.status = RouteFollowerStatus::Following;
            break;
        case game::navigation::TrajectoryFollower::Status::Complete:
            out.status = RouteFollowerStatus::Complete;
            break;
        default:
            out.status = RouteFollowerStatus::InvalidInput;
            break;
    }

    out.intent = legacy.intent;
    out.remainingDistanceMeters = legacy.remainingDistanceMeters;
    out.targetVelocityMapMps = legacy.targetVelocityMapMps;
    out.crossTrackErrorMeters = legacy.crossTrackErrorMeters;
    out.linearVelocityErrorMps = legacy.linearVelocityErrorMps;
    out.envelopePositionErrorMeters =
        legacy.envelopePositionErrorMeters;
    out.envelopeVelocityErrorMps =
        legacy.envelopeVelocityErrorMps;
    out.forwardAngleErrorRad = legacy.forwardAngleErrorRad;
    out.envelopeForwardAngleErrorRad =
        legacy.envelopeForwardAngleErrorRad;
    out.angularVelocityErrorRadPerSec =
        legacy.angularVelocityErrorRadPerSec;
    out.trackingErrorExceeded = legacy.trackingErrorExceeded;
    out.angularCorrectionOnly = legacy.angularCorrectionOnly;
    out.spatialReference = legacy.spatialReference;
    out.referenceLowerSampleIndex =
        legacy.referenceLowerSampleIndex;
    out.referenceUpperSampleIndex =
        legacy.referenceUpperSampleIndex;
    out.referenceInterpolation01 =
        legacy.referenceInterpolation01;
    out.referenceSpatialDistanceMeters =
        legacy.referenceSpatialDistanceMeters;
    out.spatialSpeedScale = legacy.spatialSpeedScale;
    return out;
}

} // namespace game::navigation::autopilot
