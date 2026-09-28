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

    auto reference = sampled.reference;
    double spatialSpeedScale = 1.0;

    // SpatialCorridor is a path-following contract, not a stopwatch.  The
    // projection above owns position progress.  A trajectory often begins at
    // a legitimate zero-speed sample; if we blindly keep that sample's zero
    // velocity until position changes, a stopped craft can never create the
    // very progress that would advance the reference.  Break that deadlock
    // from spatial data only: the current segment supplies the course and the
    // next accepted control point supplies the local launch speed.  Once the
    // craft has moved, ordinary spatial interpolation owns speed again.
    if (spatialCorridor &&
        sampled.lowerSampleIndex < sampled.upperSampleIndex &&
        sampled.upperSampleIndex < program.sampleCount)
    {
        const auto& lower =
            program.samples[sampled.lowerSampleIndex];
        const auto& upper =
            program.samples[sampled.upperSampleIndex];
        const glm::dvec3 segment =
            upper.positionMapMeters - lower.positionMapMeters;
        const double segmentLength = glm::length(segment);
        const double referenceSpeed =
            glm::length(reference.velocityMapMetersPerSecond);
        const double upperSpeed =
            glm::length(upper.velocityMapMetersPerSecond);

        if (finite(segmentLength) &&
            finite(referenceSpeed) &&
            finite(upperSpeed) &&
            segmentLength > kEpsilon &&
            referenceSpeed <= kEpsilon &&
            upperSpeed > kEpsilon)
        {
            reference.velocityMapMetersPerSecond =
                (segment / segmentLength) * upperSpeed;
        }
    }

    if (spatialCorridor &&
        program.tracking.positionErrorMeters > kEpsilon)
    {
        const double limit =
            program.tracking.positionErrorMeters;
        const double slowdownStart =
            limit *
            program.tracking.spatialSlowdownStartFraction;

        if (sampled.spatialDistanceMeters > slowdownStart &&
            limit > slowdownStart + kEpsilon)
        {
            spatialSpeedScale = std::clamp(
                (limit - sampled.spatialDistanceMeters) /
                    (limit - slowdownStart),
                0.0,
                1.0
            );
        }

        if (spatialSpeedScale < 1.0)
        {
            glm::dvec3 tangent =
                reference.velocityMapMetersPerSecond;
            double tangentLength = glm::length(tangent);
            if (!(tangentLength > kEpsilon) &&
                sampled.upperSampleIndex <
                    program.sampleCount)
            {
                tangent =
                    program.samples[sampled.upperSampleIndex].
                        positionMapMeters -
                    program.samples[sampled.lowerSampleIndex].
                        positionMapMeters;
                tangentLength = glm::length(tangent);
            }

            if (tangentLength > kEpsilon)
            {
                tangent /= tangentLength;
                const glm::dvec3 acceleration =
                    reference.
                        linearAccelerationFeedForwardMapMps2;
                const double alongAcceleration =
                    glm::dot(acceleration, tangent);
                const glm::dvec3 lateralAcceleration =
                    acceleration -
                    tangent * alongAcceleration;

                // Slowing for corridor capture may suppress planned forward
                // acceleration, but never weakens already-planned braking.
                const double governedAlongAcceleration =
                    alongAcceleration > 0.0
                        ? alongAcceleration * spatialSpeedScale
                        : alongAcceleration;

                reference.
                    linearAccelerationFeedForwardMapMps2 =
                        tangent * governedAlongAcceleration +
                        lateralAcceleration *
                            spatialSpeedScale *
                            spatialSpeedScale;
            }
            else
            {
                reference.
                    linearAccelerationFeedForwardMapMps2 =
                        glm::dvec3(0.0);
            }

            reference.velocityMapMetersPerSecond *=
                spatialSpeedScale;
            reference.angularVelocityMapRadPerSecond *=
                spatialSpeedScale;
            reference.
                angularAccelerationFeedForwardMapRadPerSec2 *=
                    spatialSpeedScale *
                    spatialSpeedScale;
        }
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
            reference,
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
        reference.velocityMapMetersPerSecond;
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
    result.spatialSpeedScale =
        spatialSpeedScale;

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
