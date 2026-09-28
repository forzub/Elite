#include "src/game/navigation/autopilot/RouteFollowerApi.h"

#include "src/game/navigation/ManeuverProgramSampler.h"
#include "src/game/navigation/ManeuverProgramTimeline.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/TrajectoryFollower.h"

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
    game::navigation::TrajectoryFollower::AgentState legacyAgent;
    const auto trackingAgent = adaptAgent(agent);
    legacyAgent.positionMapMeters = trackingAgent.positionMapMeters;
    legacyAgent.velocityMapMetersPerSecond =
        trackingAgent.velocityMapMetersPerSecond;
    legacyAgent.forwardMap = trackingAgent.forwardMap;
    legacyAgent.rightMap = trackingAgent.rightMap;
    legacyAgent.upMap = trackingAgent.upMap;
    legacyAgent.pitchRateRadPerSec = trackingAgent.pitchRateRadPerSec;
    legacyAgent.yawRateRadPerSec = trackingAgent.yawRateRadPerSec;
    legacyAgent.rollRateRadPerSec = trackingAgent.rollRateRadPerSec;

    const auto legacyPolicy = adaptPolicy(policy);

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
