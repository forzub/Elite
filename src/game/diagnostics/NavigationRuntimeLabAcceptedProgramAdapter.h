#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/AcceptedShortSegment.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/TrajectoryFollower.h"

namespace game::diagnostics
{

// Transitional adapter owned strictly by NavigationRuntimeLab.
//
// AcceptedShortSegment is no longer an executable Follower input. The lab may
// retain that historical planner product while migration work continues, but
// execution is normalized here into the single production-facing
// AcceptedManeuverProgram -> TrajectoryFollower contract.
class NavigationRuntimeLabAcceptedProgramAdapter final
{
public:
    struct Adapted
    {
        game::navigation::AcceptedManeuverProgram program {};
        game::navigation::ManeuverTrackingController::Policy trackingPolicy {};
    };

    [[nodiscard]] static Adapted adapt(
        const game::navigation::AcceptedShortSegment& segment,
        const game::navigation::TrajectoryFollower::AgentState& agent
    ) noexcept
    {
        using Segment = game::navigation::AcceptedShortSegment;
        using Program = game::navigation::AcceptedManeuverProgram;

        Adapted out;
        if (!segment.valid ||
            segment.revision == 0 ||
            !std::isfinite(segment.acceptedAtUniverseTimeSeconds) ||
            !std::isfinite(segment.validUntilUniverseTimeSeconds) ||
            segment.validUntilUniverseTimeSeconds <=
                segment.acceptedAtUniverseTimeSeconds)
        {
            return out;
        }

        const double duration =
            segment.validUntilUniverseTimeSeconds -
            segment.acceptedAtUniverseTimeSeconds;

        Program& program = out.program;
        program.valid = true;
        program.revision = segment.revision;
        program.objectiveRevision = segment.goalRevision;
        program.family = segment.emergency
            ? Program::ManeuverFamily::EmergencyRecovery
            : Program::ManeuverFamily::FreeTransit;
        program.acceptedAtUniverseTimeSeconds =
            segment.acceptedAtUniverseTimeSeconds;
        program.validUntilUniverseTimeSeconds =
            segment.validUntilUniverseTimeSeconds;
        program.sampleCount = 2;

        glm::dvec3 forward = normalizedOr(
            segment.alignForward
                ? segment.desiredForwardMap
                : agent.forwardMap,
            glm::dvec3(0.0, 0.0, -1.0)
        );
        glm::dvec3 upSeed = normalizedOr(
            agent.upMap,
            glm::dvec3(0.0, 1.0, 0.0)
        );
        upSeed -= forward * glm::dot(upSeed, forward);
        glm::dvec3 up = normalizedOr(
            upSeed,
            std::abs(forward.y) < 0.9
                ? glm::dvec3(0.0, 1.0, 0.0)
                : glm::dvec3(1.0, 0.0, 0.0)
        );
        up -= forward * glm::dot(up, forward);
        up = normalizedOr(up, glm::dvec3(0.0, 1.0, 0.0));
        const glm::dvec3 right = normalizedOr(
            glm::cross(forward, up),
            glm::dvec3(1.0, 0.0, 0.0)
        );
        up = normalizedOr(
            glm::cross(right, forward),
            up
        );

        const glm::dvec3 feedForward =
            segment.linearMode == Segment::LinearMode::FixedAcceleration
                ? segment.fixedLinearAccelerationMapMps2
                : glm::dvec3(0.0);

        for (std::size_t i = 0; i < 2; ++i)
        {
            auto& sample = program.samples[i];
            sample.timeOffsetSeconds =
                i == 0 ? 0.0 : duration;
            sample.positionMapMeters =
                i == 0
                    ? segment.startPositionMapMeters
                    : segment.targetPositionMapMeters;
            sample.velocityMapMetersPerSecond =
                segment.targetVelocityMapMetersPerSecond;
            sample.linearAccelerationFeedForwardMapMps2 =
                feedForward;
            sample.forwardMap = forward;
            sample.rightMap = right;
            sample.upMap = up;
            sample.angularVelocityMapRadPerSecond =
                glm::dvec3(0.0);
            sample.angularAccelerationFeedForwardMapRadPerSec2 =
                glm::dvec3(0.0);
        }

        // Legacy lab completion and cross-track semantics are preserved by the
        // adapter helpers below. The program itself therefore never invents a
        // second completion/envelope policy.
        program.completionTriggersReplan = false;
        program.terminalTolerance.positionMeters =
            segment.completionRadiusMeters;
        program.terminalTolerance.linearVelocityMps = 1.0e12;
        program.terminalTolerance.forwardAngleRad =
            3.14159265358979323846;
        program.terminalTolerance.angularVelocityRadPerSec =
            1.0e12;

        program.tracking.positionErrorMeters = 0.0;
        program.tracking.linearVelocityErrorMps = 0.0;
        program.tracking.forwardAngleErrorRad = 0.0;
        program.tracking.angularVelocityErrorRadPerSec = 0.0;
        program.tracking.alongTrackPositionDeadbandMeters = 1.0e12;
        program.tracking.alongTrackSpeedDeadbandMps = 1.0e12;

        const double linearAuthority = std::max({
            segment.capability.maxForwardAccelerationMetersPerSec2,
            segment.capability.maxReverseAccelerationMetersPerSec2,
            segment.capability.maxLateralAccelerationMetersPerSec2,
            segment.capability.maxVerticalAccelerationMetersPerSec2,
            glm::length(feedForward),
            1.0
        });
        program.tracking.linearFeedbackReserveMps2 =
            linearAuthority * 4.0;

        const double angularAuthority = std::max(
            segment.maximumAngularAccelerationRadPerSec2,
            1.0
        );
        program.tracking.angularFeedbackReserveRadPerSec2 =
            angularAuthority;

        program.capability.maxForwardAccelerationMetersPerSec2 =
            segment.capability.maxForwardAccelerationMetersPerSec2;
        program.capability.maxReverseAccelerationMetersPerSec2 =
            segment.capability.maxReverseAccelerationMetersPerSec2;
        program.capability.maxLateralAccelerationMetersPerSec2 =
            segment.capability.maxLateralAccelerationMetersPerSec2;
        program.capability.maxVerticalAccelerationMetersPerSec2 =
            segment.capability.maxVerticalAccelerationMetersPerSec2;
        program.capability.maxAngularAccelerationRadPerSec2 =
            segment.capability.maxAngularAccelerationRadPerSec2;
        program.capability.maxAngularSpeedRadPerSec =
            segment.capability.maxAngularSpeedRadPerSec;

        program.proof.mapRevision = segment.mapRevision;
        program.proof.mapSourceRevision = segment.mapSourceRevision;
        program.proof.spaceRevision = segment.spaceRevision;
        program.proof.spaceSourceRevision = segment.spaceSourceRevision;
        program.emergency = segment.emergency;
        program.hazardUrgency01 =
            std::clamp(segment.hazardUrgency01, 0.0, 1.0);

        out.trackingPolicy.positionGainPerSecond2 = 0.0;
        out.trackingPolicy.velocityGainPerSecond =
            segment.linearMode == Segment::LinearMode::VelocityTracking
                ? segment.velocityResponsePerSecond
                : 0.0;
        out.trackingPolicy.attitudeGainPerSecond2 =
            segment.alignForward
                ? segment.orientationResponsePerSecond2
                : 0.0;
        out.trackingPolicy.angularVelocityGainPerSecond =
            segment.angularDampingPerSecond;

        return out;
    }

    [[nodiscard]] static double crossTrackErrorMeters(
        const game::navigation::AcceptedShortSegment& segment,
        const game::navigation::TrajectoryFollower::AgentState& agent
    ) noexcept
    {
        const glm::dvec3 path =
            segment.targetPositionMapMeters -
            segment.startPositionMapMeters;
        const double lengthSquared = glm::dot(path, path);
        if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12)
        {
            return glm::length(
                agent.positionMapMeters -
                segment.startPositionMapMeters
            );
        }

        const double t = std::clamp(
            glm::dot(
                agent.positionMapMeters -
                    segment.startPositionMapMeters,
                path
            ) / lengthSquared,
            0.0,
            1.0
        );
        return glm::length(
            agent.positionMapMeters -
            (segment.startPositionMapMeters + path * t)
        );
    }

    static void preserveLegacyMonitoring(
        const game::navigation::AcceptedShortSegment& segment,
        const game::navigation::TrajectoryFollower::AgentState& agent,
        game::navigation::TrajectoryFollower::Result& result
    ) noexcept
    {
        if (result.status ==
            game::navigation::TrajectoryFollower::Status::InvalidInput)
        {
            return;
        }

        result.remainingDistanceMeters = glm::length(
            segment.targetPositionMapMeters -
            agent.positionMapMeters
        );
        result.crossTrackErrorMeters =
            crossTrackErrorMeters(segment, agent);
        result.trackingErrorExceeded =
            segment.trackingEnvelopeRadiusMeters > 0.0 &&
            result.crossTrackErrorMeters >
                segment.trackingEnvelopeRadiusMeters;

        if (segment.completionTriggersReplan &&
            result.remainingDistanceMeters <=
                segment.completionRadiusMeters)
        {
            result.status =
                game::navigation::TrajectoryFollower::Status::Complete;
        }
    }

private:
    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double lengthSquared = glm::dot(value, value);
        if (!std::isfinite(lengthSquared) ||
            lengthSquared <= 1.0e-12)
        {
            return fallback;
        }
        return value / std::sqrt(lengthSquared);
    }
};

} // namespace game::diagnostics
