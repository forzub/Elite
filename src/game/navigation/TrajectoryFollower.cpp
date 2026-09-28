#include "TrajectoryFollower.h"

#include "src/game/navigation/ManeuverProgramSampler.h"
#include "src/game/navigation/ManeuverProgramTimeline.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

} // namespace

TrajectoryFollower::Result TrajectoryFollower::follow(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds,
    const AgentState& agent,
    const ManeuverTrackingController::Policy& trackingPolicy,
    std::size_t minimumSpatialSegmentIndex
) noexcept
{
    Result result;

    const bool spatialCorridor =
        program.referenceMode ==
            AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;

    const auto sampled =
        spatialCorridor
            ? ManeuverProgramSampler::sampleSpatial(
                  program,
                  universeTimeSeconds,
                  agent.positionMapMeters,
                  minimumSpatialSegmentIndex
              )
            : ManeuverProgramSampler::sample(
                  program,
                  universeTimeSeconds
              );
    if (sampled.status ==
            ManeuverProgramSampler::Status::InvalidInput ||
        sampled.status ==
            ManeuverProgramSampler::Status::BeforeStart)
    {
        return result;
    }

    ManeuverTrackingController::AgentState trackingAgent;
    trackingAgent.positionMapMeters = agent.positionMapMeters;
    trackingAgent.velocityMapMetersPerSecond =
        agent.velocityMapMetersPerSecond;
    trackingAgent.forwardMap = agent.forwardMap;
    trackingAgent.rightMap = agent.rightMap;
    trackingAgent.upMap = agent.upMap;
    trackingAgent.pitchRateRadPerSec = agent.pitchRateRadPerSec;
    trackingAgent.yawRateRadPerSec = agent.yawRateRadPerSec;
    trackingAgent.rollRateRadPerSec = agent.rollRateRadPerSec;

    const auto tracking =
        ManeuverTrackingController::track(
            program,
            sampled.reference,
            trackingAgent,
            trackingPolicy
        );

    if (tracking.status ==
        ManeuverTrackingController::Status::InvalidInput)
    {
        return result;
    }

    result.intent = tracking.intent;
    result.targetVelocityMapMps =
        sampled.reference.velocityMapMetersPerSecond;
    result.crossTrackErrorMeters =
        tracking.positionErrorMeters;
    result.linearVelocityErrorMps =
        tracking.linearVelocityErrorMps;
    result.envelopePositionErrorMeters =
        tracking.envelopePositionErrorMeters;
    result.envelopeVelocityErrorMps =
        tracking.envelopeVelocityErrorMps;
    result.forwardAngleErrorRad =
        tracking.forwardAngleErrorRad;
    result.angularVelocityErrorRadPerSec =
        tracking.angularVelocityErrorRadPerSec;
    result.trackingErrorExceeded =
        tracking.status ==
        ManeuverTrackingController::Status::EnvelopeExceeded;
    result.angularCorrectionOnly = tracking.angularCorrectionOnly;
    result.spatialReference = sampled.spatialReference;
    result.referenceLowerSampleIndex =
        sampled.lowerSampleIndex;
    result.referenceUpperSampleIndex =
        sampled.upperSampleIndex;
    result.referenceInterpolation01 =
        sampled.interpolation01;
    result.referenceSpatialDistanceMeters =
        sampled.spatialDistanceMeters;

    const std::size_t lastIndex =
        static_cast<std::size_t>(program.sampleCount - 1);
    result.remainingDistanceMeters =
        glm::length(
            program.samples[lastIndex].positionMapMeters -
            agent.positionMapMeters
        );

    if (!finite(result.remainingDistanceMeters))
        return Result {};

    const double endOffset =
        program.samples[lastIndex].timeOffsetSeconds;
    const double elapsed =
        ManeuverProgramTimeline::elapsedPageSeconds(
            program,
            universeTimeSeconds
        );
    if (!finite(elapsed))
        return Result {};
    const bool atOrAfterProgramEnd =
        elapsed >= endOffset - kEpsilon;

    const bool terminalSatisfied =
        result.remainingDistanceMeters <=
            program.terminalTolerance.positionMeters &&
        result.linearVelocityErrorMps <=
            program.terminalTolerance.linearVelocityMps &&
        result.forwardAngleErrorRad <=
            program.terminalTolerance.forwardAngleRad &&
        result.angularVelocityErrorRadPerSec <=
            program.terminalTolerance.angularVelocityRadPerSec;

    // Spatial corridors are progress-driven. Reaching the accepted terminal
    // state early is success; nominal trajectory time is not allowed to hold
    // the craft back or drag the reference ahead. TimeScheduled maneuvers keep
    // their original completion clock semantics.
    const bool completionClockSatisfied =
        spatialCorridor || atOrAfterProgramEnd;

    result.status =
        program.completionTriggersReplan &&
        completionClockSatisfied &&
        terminalSatisfied
            ? Status::Complete
            : Status::Following;

    return result;
}

} // namespace game::navigation
