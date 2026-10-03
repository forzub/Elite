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
    glm::dvec3 targetVelocity =
        reference.velocityMapMetersPerSecond;

    // V2 corridor doctrine:
    // - the accepted centerline is the ONLY path;
    // - no look-ahead ray may invent a second local route;
    // - correction is a velocity component directed from the real craft
    //   straight back toward the current centerline segment.
    //
    // This can move diagonally inside the authored corridor, but it can never
    // place a steering target beside the corridor or skip across a corner.
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

        if (std::isfinite(segmentLength) &&
            segmentLength > 1.0e-12 &&
            std::isfinite(referenceSpeed) &&
            referenceSpeed > 1.0e-9)
        {
            const glm::dvec3 tangent = segment / segmentLength;

            const double progress = std::clamp(
                glm::dot(
                    agent.positionMapMeters - lower.positionMapMeters,
                    segment
                ) / (segmentLength * segmentLength),
                0.0,
                1.0
            );
            const glm::dvec3 centerlinePoint =
                lower.positionMapMeters + segment * progress;

            glm::dvec3 inward =
                centerlinePoint - agent.positionMapMeters;
            inward -= tangent * glm::dot(inward, tangent);
            const double crossTrack = glm::length(inward);

            if (std::isfinite(crossTrack) && crossTrack > 1.0e-6)
            {
                inward /= crossTrack;

                // Use only the feedback authority already reserved by the
                // accepted program. The correction speed is braking-limited:
                // if correction stopped now, the ship can still arrest that
                // lateral motion before crossing the same error distance.
                const double reserve = std::max(
                    0.0,
                    program.tracking.linearFeedbackReserveMps2
                );
                const double brakingLimitedCorrection =
                    reserve > 1.0e-9
                        ? std::sqrt(2.0 * reserve * crossTrack)
                        : 0.0;

                // Never spend more than 35% of route speed laterally. Forward
                // progress remains monotonic and corner cutting cannot become
                // a substitute for following the authored centerline.
                const double correctionSpeed = std::min(
                    referenceSpeed * 0.35,
                    brakingLimitedCorrection
                );
                const double forwardSpeed = std::sqrt(
                    std::max(
                        0.0,
                        referenceSpeed * referenceSpeed -
                            correctionSpeed * correctionSpeed
                    )
                );

                targetVelocity =
                    tangent * forwardSpeed +
                    inward * correctionSpeed;
            }
            else
            {
                targetVelocity = tangent * referenceSpeed;
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
        measuredSpeed <= policy.velocityGainPerSecond)
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
