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

    glm::dvec3 targetVelocityMapMps =
        reference.velocityMapMetersPerSecond;

    // SpatialCorridor is geometric guidance. Cross-track error must change
    // COURSE, not silently reduce the route speed. Aim the hull and the
    // Assisted velocity target at a bounded look-ahead point on the SAME
    // accepted path. ManeuverTrackingController still derives its spatial
    // envelope tangent from reference.velocity, so route-loss remains measured
    // against the authored corridor rather than this corrective steering ray.
    if (spatialCorridor &&
        sampled.lowerSampleIndex < sampled.upperSampleIndex &&
        sampled.upperSampleIndex < program.sampleCount)
    {
        const double actualSpeed =
            glm::length(agent.velocityMapMetersPerSecond);
        const double lookAheadMeters = std::clamp(
            std::max(50.0, actualSpeed * 1.5),
            50.0,
            250.0
        );

        glm::dvec3 lookAheadPoint =
            program.samples[sampled.upperSampleIndex].
                positionMapMeters;
        double accumulated = glm::length(
            lookAheadPoint - reference.positionMapMeters
        );

        for (std::size_t i = sampled.upperSampleIndex;
             i + 1 < program.sampleCount &&
             accumulated < lookAheadMeters;
             ++i)
        {
            const glm::dvec3 a =
                program.samples[i].positionMapMeters;
            const glm::dvec3 b =
                program.samples[i + 1].positionMapMeters;
            const double segmentLength = glm::length(b - a);
            if (!(segmentLength > kEpsilon) ||
                !finite(segmentLength))
            {
                continue;
            }

            const double remaining =
                lookAheadMeters - accumulated;
            if (remaining < segmentLength)
            {
                lookAheadPoint =
                    a + (b - a) * (remaining / segmentLength);
                accumulated = lookAheadMeters;
                break;
            }

            accumulated += segmentLength;
            lookAheadPoint = b;
        }

        const glm::dvec3 steeringRay =
            lookAheadPoint - agent.positionMapMeters;
        const double steeringDistance =
            glm::length(steeringRay);
        const double referenceSpeed =
            glm::length(reference.velocityMapMetersPerSecond);

        if (steeringDistance > kEpsilon &&
            finite(steeringDistance) &&
            finite(referenceSpeed))
        {
            glm::dvec3 desiredForward =
                steeringRay / steeringDistance;

            // Avoid visible hunting on a straight segment. Tiny positional
            // noise must not create a new nose target every fixed step. Once
            // the craft is inside a small central band, hold the authored
            // segment tangent exactly; outside that band, retain the stronger
            // inward look-ahead steering used for real corridor recovery.
            const auto& lower =
                program.samples[sampled.lowerSampleIndex];
            const auto& upper =
                program.samples[sampled.upperSampleIndex];
            const glm::dvec3 segment =
                upper.positionMapMeters -
                lower.positionMapMeters;
            const double segmentLength =
                glm::length(segment);

            if (segmentLength > kEpsilon &&
                finite(segmentLength))
            {
                const glm::dvec3 tangent =
                    segment / segmentLength;
                const double rawProgress =
                    glm::dot(
                        agent.positionMapMeters -
                            lower.positionMapMeters,
                        segment
                    ) / (segmentLength * segmentLength);
                const double clampedProgress =
                    std::clamp(rawProgress, 0.0, 1.0);
                const glm::dvec3 projected =
                    lower.positionMapMeters +
                    segment * clampedProgress;
                const double crossTrackMeters =
                    glm::length(
                        agent.positionMapMeters - projected
                    );
                const double centerDeadbandMeters =
                    std::clamp(
                        program.tracking.positionErrorMeters * 0.10,
                        1.0,
                        4.0
                    );

                if (finite(crossTrackMeters) &&
                    crossTrackMeters <= centerDeadbandMeters)
                {
                    desiredForward = tangent;
                }
            }

            targetVelocityMapMps =
                desiredForward * referenceSpeed;

            // Course correction owns yaw/pitch while retaining a stable roll
            // reference. This makes Assisted behave like the agreed aircraft
            // model: nose points back into the tunnel and velocity follows.
            glm::dvec3 up =
                reference.upMap -
                desiredForward *
                    glm::dot(reference.upMap, desiredForward);
            double upLength = glm::length(up);
            if (!(upLength > kEpsilon))
            {
                up =
                    agent.upMap -
                    desiredForward *
                        glm::dot(agent.upMap, desiredForward);
                upLength = glm::length(up);
            }
            if (!(upLength > kEpsilon))
            {
                const glm::dvec3 seed =
                    std::abs(desiredForward.y) < 0.90
                        ? glm::dvec3(0.0, 1.0, 0.0)
                        : glm::dvec3(1.0, 0.0, 0.0);
                up =
                    seed -
                    desiredForward *
                        glm::dot(seed, desiredForward);
                upLength = glm::length(up);
            }

            if (upLength > kEpsilon)
            {
                up /= upLength;
                const glm::dvec3 right =
                    glm::normalize(
                        glm::cross(desiredForward, up)
                    );
                up = glm::normalize(
                    glm::cross(right, desiredForward)
                );

                reference.forwardMap = desiredForward;
                reference.rightMap = right;
                reference.upMap = up;
            }
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
        targetVelocityMapMps;
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
    result.envelopeForwardAngleErrorRad =
        tracking.envelopeForwardAngleErrorRad;
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
