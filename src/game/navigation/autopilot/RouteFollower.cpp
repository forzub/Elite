#include "src/game/navigation/autopilot/RouteFollowerApi.h"

#include "src/game/navigation/ManeuverProgramSampler.h"
#include "src/game/navigation/ManeuverProgramTimeline.h"
#include "src/game/navigation/ManeuverTrackingController.h"

#include <algorithm>
#include <cmath>

namespace game::navigation::autopilot
{
namespace
{

game::navigation::ManeuverTrackingController::AgentState adaptAgent(
    const RouteFollowerAgentState& agent
) noexcept
{
    game::navigation::ManeuverTrackingController::AgentState out;
    out.positionMapMeters = agent.positionMapMeters;
    out.velocityMapMetersPerSecond = agent.velocityMapMetersPerSecond;
    out.forwardMap = agent.forwardMap;
    out.rightMap = agent.rightMap;
    out.upMap = agent.upMap;
    out.pitchRateRadPerSec = agent.pitchRateRadPerSec;
    out.yawRateRadPerSec = agent.yawRateRadPerSec;
    out.rollRateRadPerSec = agent.rollRateRadPerSec;
    return out;
}

game::navigation::ManeuverTrackingController::Policy adaptPolicy(
    const RouteFollowerPolicy& policy
) noexcept
{
    game::navigation::ManeuverTrackingController::Policy out;
    out.positionGainPerSecond2 = policy.positionGainPerSecond2;
    out.velocityGainPerSecond = policy.velocityGainPerSecond;
    out.attitudeGainPerSecond2 = policy.attitudeGainPerSecond2;
    out.angularVelocityGainPerSecond =
        policy.angularVelocityGainPerSecond;
    return out;
}

double axisAngle(
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    const double la = glm::length(a);
    const double lb = glm::length(b);
    if (!(std::isfinite(la) && std::isfinite(lb)) ||
        la <= 1.0e-12 || lb <= 1.0e-12)
    {
        return 3.14159265358979323846;
    }

    return std::acos(
        std::clamp(glm::dot(a / la, b / lb), -1.0, 1.0)
    );
}

} // namespace

RouteAlignmentResult RouteFollower::alignToAttitude(
    const AcceptedManeuverProgram& capabilityProgram,
    const RouteFollowerAgentState& agent,
    const glm::dvec3& desiredForwardMap,
    const glm::dvec3& desiredRightMap,
    const glm::dvec3& desiredUpMap,
    const RouteFollowerPolicy& policy
) noexcept
{
    RouteAlignmentResult out;
    if (!capabilityProgram.valid ||
        capabilityProgram.sampleCount == 0)
    {
        return out;
    }

    auto reference = capabilityProgram.samples[0];
    reference.positionMapMeters = agent.positionMapMeters;
    reference.velocityMapMetersPerSecond =
        agent.velocityMapMetersPerSecond;
    reference.linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);
    reference.forwardMap = desiredForwardMap;
    reference.rightMap = desiredRightMap;
    reference.upMap = desiredUpMap;
    reference.angularVelocityMapRadPerSecond = glm::dvec3(0.0);
    reference.angularAccelerationFeedForwardMapRadPerSec2 =
        glm::dvec3(0.0);

    auto alignmentProgram = capabilityProgram;
    alignmentProgram.family =
        AcceptedManeuverProgram::ManeuverFamily::PrecisionTransit;
    alignmentProgram.tracking.positionErrorMeters = 0.0;
    alignmentProgram.tracking.linearVelocityErrorMps = 0.0;
    alignmentProgram.tracking.forwardAngleErrorRad = 0.0;
    alignmentProgram.tracking.angularVelocityErrorRadPerSec = 0.0;
    alignmentProgram.tracking.linearFeedbackReserveMps2 = 0.0;

    const auto tracking =
        game::navigation::ManeuverTrackingController::track(
            alignmentProgram,
            reference,
            adaptAgent(agent),
            adaptPolicy(policy)
        );
    if (tracking.status ==
        game::navigation::ManeuverTrackingController::Status::InvalidInput)
    {
        return out;
    }

    out.valid = true;
    out.intent = tracking.intent;
    out.forwardAngleErrorRad =
        axisAngle(agent.forwardMap, desiredForwardMap);
    out.upAngleErrorRad =
        axisAngle(agent.upMap, desiredUpMap);
    return out;
}


RouteProgramSelection RouteFollower::selectPage(
    const std::vector<AcceptedManeuverProgram>& pages,
    double universeTimeSeconds,
    const glm::dvec3& positionMapMeters,
    std::size_t currentPageIndex
) noexcept
{
    RouteProgramSelection out;
    if (pages.empty() || currentPageIndex >= pages.size())
        return out;

    const bool spatial =
        pages[currentPageIndex].referenceMode ==
            AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;

    const auto selected =
        spatial
            ? game::navigation::ManeuverProgramTimeline::selectSpatialPage(
                  pages.data(),
                  pages.size(),
                  universeTimeSeconds,
                  positionMapMeters,
                  currentPageIndex
              )
            : game::navigation::ManeuverProgramTimeline::selectActivePage(
                  pages.data(),
                  pages.size(),
                  universeTimeSeconds,
                  currentPageIndex
              );

    switch (selected.status)
    {
        case game::navigation::ManeuverProgramTimeline::
            SelectionStatus::BeforeStart:
            out.status = RouteProgramSelectionStatus::BeforeStart;
            break;
        case game::navigation::ManeuverProgramTimeline::
            SelectionStatus::Active:
            out.status = RouteProgramSelectionStatus::Active;
            break;
        default:
            out.status = RouteProgramSelectionStatus::InvalidInput;
            break;
    }

    out.pageIndex = selected.pageIndex;
    out.pagesAdvanced = selected.pagesAdvanced;
    const auto firstWindow =
        game::navigation::ManeuverProgramTimeline::pageWindow(
            pages.front()
        );
    if (firstWindow.valid)
    {
        out.firstPageStartUniverseTimeSeconds =
            firstWindow.startUniverseTimeSeconds;
    }
    return out;
}

RouteReferenceDiagnostic RouteFollower::sampleReference(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds,
    const glm::dvec3& positionMapMeters,
    std::size_t minimumSpatialSegmentIndex
) noexcept
{
    const auto sampled =
        program.referenceMode ==
                AcceptedManeuverProgram::ReferenceMode::SpatialCorridor
            ? game::navigation::ManeuverProgramSampler::sampleSpatial(
                  program,
                  universeTimeSeconds,
                  positionMapMeters,
                  minimumSpatialSegmentIndex
              )
            : game::navigation::ManeuverProgramSampler::sample(
                  program,
                  universeTimeSeconds
              );

    RouteReferenceDiagnostic out;
    out.valid =
        sampled.status !=
            game::navigation::ManeuverProgramSampler::Status::InvalidInput &&
        sampled.status !=
            game::navigation::ManeuverProgramSampler::Status::BeforeStart;
    out.spatialReference = sampled.spatialReference;
    out.reference = sampled.reference;
    out.lowerSampleIndex = sampled.lowerSampleIndex;
    out.upperSampleIndex = sampled.upperSampleIndex;
    out.interpolation01 = sampled.interpolation01;
    out.spatialDistanceMeters = sampled.spatialDistanceMeters;
    return out;
}

RouteFollowerResult RouteFollower::follow(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds,
    const RouteFollowerAgentState& agent,
    const RouteFollowerPolicy& policy,
    std::size_t minimumSpatialSegmentIndex
) noexcept
{
    RouteFollowerResult out;

    const bool spatialCorridor =
        program.referenceMode ==
            AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;

    const auto sampled =
        spatialCorridor
            ? game::navigation::ManeuverProgramSampler::sampleSpatial(
                  program,
                  universeTimeSeconds,
                  agent.positionMapMeters,
                  minimumSpatialSegmentIndex
              )
            : game::navigation::ManeuverProgramSampler::sample(
                  program,
                  universeTimeSeconds
              );

    if (sampled.status ==
            game::navigation::ManeuverProgramSampler::Status::InvalidInput ||
        sampled.status ==
            game::navigation::ManeuverProgramSampler::Status::BeforeStart)
    {
        return out;
    }

    auto reference = sampled.reference;

    // Restore the proven SpatialCorridor launch contract from
    // 2608765f48.  A zero-speed first sample is the physical initial
    // condition, not a permanent hold command.  Spatial progress cannot move
    // until the ship moves, so at the exact origin use the accepted next
    // control point's speed along the current segment.  Once position advances,
    // ordinary spatial interpolation takes over again.
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

        if (std::isfinite(segmentLength) &&
            std::isfinite(referenceSpeed) &&
            std::isfinite(upperSpeed) &&
            segmentLength > 1.0e-12 &&
            referenceSpeed <= 1.0e-12 &&
            upperSpeed > 1.0e-12)
        {
            reference.velocityMapMetersPerSecond =
                (segment / segmentLength) * upperSpeed;
        }
    }

    glm::dvec3 targetVelocity =
        reference.velocityMapMetersPerSecond;

    // SpatialCorridor is geometric guidance. Use the proven corridor
    // look-ahead steering law: choose a point AHEAD ON THE SAME accepted
    // centerline, aim the hull at it, and give Assisted the same velocity
    // direction. This is not a second/private route; the target point is
    // constrained to the authored corridor itself.
    //
    // Crucially, this produces anticipatory steering before a bend. The
    // local-segment tangent + perpendicular correction law that replaced this
    // behaviour waits until the active segment itself turns, which is too late
    // for a ship with finite angular response.
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
            if (!(segmentLength > 1.0e-12) ||
                !std::isfinite(segmentLength))
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

        if (steeringDistance > 1.0e-12 &&
            std::isfinite(steeringDistance) &&
            std::isfinite(referenceSpeed))
        {
            glm::dvec3 desiredForward =
                steeringRay / steeringDistance;

            // On a straight, tiny projection noise must not make the nose
            // hunt. Inside the central deadband retain the current segment
            // tangent exactly. Outside it, the look-ahead point pulls the ship
            // back through the visible tunnel just as the old follower did.
            const auto& lower =
                program.samples[sampled.lowerSampleIndex];
            const auto& upper =
                program.samples[sampled.upperSampleIndex];
            const glm::dvec3 segment =
                upper.positionMapMeters -
                lower.positionMapMeters;
            const double segmentLength =
                glm::length(segment);

            if (segmentLength > 1.0e-12 &&
                std::isfinite(segmentLength))
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

                // Preserve the exact tangent only when the look-ahead target
                // is still effectively collinear. Near an upcoming bend the
                // point ahead has already moved off that tangent, so allow the
                // anticipatory steering angle even while cross-track is tiny.
                const double lookAheadOffTangent =
                    glm::length(
                        steeringRay -
                        tangent * glm::dot(steeringRay, tangent)
                    );

                if (std::isfinite(crossTrackMeters) &&
                    std::isfinite(lookAheadOffTangent) &&
                    crossTrackMeters <= centerDeadbandMeters &&
                    lookAheadOffTangent <= centerDeadbandMeters)
                {
                    desiredForward = tangent;
                }
            }

            targetVelocity =
                desiredForward * referenceSpeed;

            glm::dvec3 up =
                reference.upMap -
                desiredForward *
                    glm::dot(reference.upMap, desiredForward);
            double upLength = glm::length(up);
            if (!(upLength > 1.0e-12))
            {
                up =
                    agent.upMap -
                    desiredForward *
                        glm::dot(agent.upMap, desiredForward);
                upLength = glm::length(up);
            }
            if (!(upLength > 1.0e-12))
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

            if (upLength > 1.0e-12)
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

    auto tracking =
        game::navigation::ManeuverTrackingController::track(
            program,
            reference,
            adaptAgent(agent),
            adaptPolicy(policy)
        );

    const double authoredReferenceSpeed =
        glm::length(reference.velocityMapMetersPerSecond);
    const double measuredSpeed =
        glm::length(agent.velocityMapMetersPerSecond);

    if (std::isfinite(authoredReferenceSpeed) &&
        std::isfinite(measuredSpeed) &&
        authoredReferenceSpeed <= 1.0e-12 &&
        measuredSpeed <= 0.50)
    {
        tracking.intent.precisionTranslationOnly = true;
    }

    if (tracking.status ==
        game::navigation::ManeuverTrackingController::Status::InvalidInput)
    {
        return out;
    }

    out.status = RouteFollowerStatus::Following;
    out.intent = tracking.intent;
    out.targetVelocityMapMps = targetVelocity;
    out.crossTrackErrorMeters = tracking.positionErrorMeters;
    out.linearVelocityErrorMps = tracking.linearVelocityErrorMps;
    out.envelopePositionErrorMeters =
        tracking.envelopePositionErrorMeters;
    out.envelopeVelocityErrorMps =
        tracking.envelopeVelocityErrorMps;
    out.forwardAngleErrorRad = tracking.forwardAngleErrorRad;
    out.envelopeForwardAngleErrorRad =
        tracking.envelopeForwardAngleErrorRad;
    out.angularVelocityErrorRadPerSec =
        tracking.angularVelocityErrorRadPerSec;
    out.trackingErrorExceeded =
        tracking.status ==
            game::navigation::ManeuverTrackingController::Status::EnvelopeExceeded;
    out.angularCorrectionOnly = tracking.angularCorrectionOnly;
    out.spatialReference = sampled.spatialReference;
    out.referenceLowerSampleIndex = sampled.lowerSampleIndex;
    out.referenceUpperSampleIndex = sampled.upperSampleIndex;
    out.referenceInterpolation01 = sampled.interpolation01;
    out.referenceSpatialDistanceMeters = sampled.spatialDistanceMeters;
    out.spatialSpeedScale = 1.0;

    const std::size_t lastIndex =
        static_cast<std::size_t>(program.sampleCount - 1);
    out.remainingDistanceMeters = glm::length(
        program.samples[lastIndex].positionMapMeters -
        agent.positionMapMeters
    );

    if (!std::isfinite(out.remainingDistanceMeters))
        return RouteFollowerResult {};

    const double elapsed =
        game::navigation::ManeuverProgramTimeline::elapsedPageSeconds(
            program,
            universeTimeSeconds
        );
    if (!std::isfinite(elapsed))
        return RouteFollowerResult {};

    const bool atOrAfterProgramEnd =
        elapsed >=
        program.samples[lastIndex].timeOffsetSeconds - 1.0e-12;

    const bool terminalSatisfied =
        out.remainingDistanceMeters <=
            program.terminalTolerance.positionMeters &&
        out.linearVelocityErrorMps <=
            program.terminalTolerance.linearVelocityMps &&
        out.forwardAngleErrorRad <=
            program.terminalTolerance.forwardAngleRad &&
        out.angularVelocityErrorRadPerSec <=
            program.terminalTolerance.angularVelocityRadPerSec;

    const bool completionClockSatisfied =
        spatialCorridor || atOrAfterProgramEnd;

    if (program.completionTriggersReplan &&
        completionClockSatisfied &&
        terminalSatisfied)
    {
        out.status = RouteFollowerStatus::Complete;
    }

    return out;
}

} // namespace game::navigation::autopilot
