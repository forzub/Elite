#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/navigation/Trajectory.h"

namespace game::navigation
{

// Production adapter from one already collision-checked, time-parameterized
// trajectory into the immutable Planner -> Follower execution product.
//
// It does NOT search geometry or change the trajectory. Every accepted page is
// a consecutive slice of the same trajectory and carries the same objective /
// proof provenance. Page boundaries are storage boundaries only.
class AcceptedManeuverProgramBuilder final
{
public:
    struct Policy
    {
        double validityGraceSeconds = 2.0;

        double terminalPositionToleranceMeters = 3.0;
        double terminalSpeedToleranceMps = 0.75;
        double terminalForwardToleranceRad = 0.08726646259971647; // 5 deg
        double terminalAngularVelocityToleranceRadPerSec = 0.05;

        double trackingPositionErrorMeters = 25.0;
        double trackingLinearVelocityErrorMps = 8.0;
        double trackingForwardAngleErrorRad = 0.2617993877991494; // 15 deg
        double trackingAngularVelocityErrorRadPerSec = 0.25;
        double alongTrackPositionDeadbandMeters = 10.0;
        double alongTrackSpeedDeadbandMps = 1.0;

        // These reserves must already have been withheld from trajectory
        // generation. The builder records them; it does not manufacture extra
        // vehicle authority.
        double linearFeedbackReserveMps2 = 0.0;
        double angularFeedbackReserveRadPerSec2 = 0.0;

        bool valid() const noexcept
        {
            const auto nonNegative = [](double v)
            {
                return std::isfinite(v) && v >= 0.0;
            };
            return
                nonNegative(validityGraceSeconds) &&
                nonNegative(terminalPositionToleranceMeters) &&
                nonNegative(terminalSpeedToleranceMps) &&
                nonNegative(terminalForwardToleranceRad) &&
                nonNegative(terminalAngularVelocityToleranceRadPerSec) &&
                nonNegative(trackingPositionErrorMeters) &&
                nonNegative(trackingLinearVelocityErrorMps) &&
                nonNegative(trackingForwardAngleErrorRad) &&
                nonNegative(trackingAngularVelocityErrorRadPerSec) &&
                nonNegative(alongTrackPositionDeadbandMeters) &&
                nonNegative(alongTrackSpeedDeadbandMps) &&
                nonNegative(linearFeedbackReserveMps2) &&
                nonNegative(angularFeedbackReserveRadPerSec2);
        }
    };

    struct Request
    {
        const world::navigation::Trajectory* trajectory = nullptr;
        const ShipParams* shipPhysics = nullptr;

        std::uint64_t objectiveRevision = 0;
        std::uint64_t firstProgramRevision = 1;
        std::uint64_t capabilityRevision = 1;

        std::uint64_t mapRevision = 0;
        std::uint64_t mapSourceRevision = 0;
        std::uint64_t spaceRevision = 0;
        std::uint64_t spaceSourceRevision = 0;

        double minimumClearanceMeters = 0.0;
        Policy policy {};
    };

    struct Result
    {
        bool valid = false;
        std::vector<AcceptedManeuverProgram> pages;
    };

    [[nodiscard]] static Result build(const Request& request)
    {
        Result out;
        if (!request.trajectory ||
            !request.shipPhysics ||
            !request.trajectory->ready() ||
            request.objectiveRevision == 0 ||
            request.firstProgramRevision == 0 ||
            !request.policy.valid() ||
            !std::isfinite(request.minimumClearanceMeters) ||
            request.minimumClearanceMeters < 0.0)
        {
            return out;
        }

        const auto& trajectory = *request.trajectory;
        const auto& samples = trajectory.samples;
        if (samples.size() < 2)
            return out;

        const ShipParams& params = *request.shipPhysics;
        const double forwardMain =
            game::ship::forwardMainAccelerationLimitMps2(params);
        const double reverseMain =
            game::ship::reverseMainAccelerationLimitMps2(params);
        const double manoeuvre =
            game::ship::manoeuvreAccelerationLimitMps2(params);

        std::size_t first = 0;
        std::uint64_t revision = request.firstProgramRevision;

        while (first + 1 < samples.size())
        {
            const std::size_t last = std::min(
                samples.size() - 1,
                first + AcceptedManeuverProgram::kMaxSamples - 1
            );

            AcceptedManeuverProgram page;
            page.valid = true;
            page.revision = revision++;
            page.objectiveRevision = request.objectiveRevision;
            page.family =
                last + 1 == samples.size()
                    ? AcceptedManeuverProgram::ManeuverFamily::PrecisionCapture
                    : AcceptedManeuverProgram::ManeuverFamily::FreeTransit;
            page.acceptedAtUniverseTimeSeconds =
                trajectory.startUniverseTimeSeconds;

            const double sourceStart =
                samples[first].timeOffsetSeconds;
            page.sequenceStartOffsetSeconds = sourceStart;

            const std::size_t count = last - first + 1;
            page.sampleCount = static_cast<std::uint8_t>(count);

            for (std::size_t i = 0; i < count; ++i)
            {
                const auto& source = samples[first + i];
                auto& target = page.samples[i];

                target.timeOffsetSeconds =
                    source.timeOffsetSeconds - sourceStart;
                target.positionMapMeters = source.positionMeters;
                target.velocityMapMetersPerSecond = source.velocityMps;
                target.linearAccelerationFeedForwardMapMps2 =
                    source.accelerationMps2;

                const glm::dquat q = normalizedOrIdentity(
                    source.orientation
                );
                target.rightMap =
                    q * glm::dvec3(1.0, 0.0, 0.0);
                target.upMap =
                    q * glm::dvec3(0.0, 1.0, 0.0);
                target.forwardMap =
                    q * glm::dvec3(0.0, 0.0, -1.0);
                target.angularVelocityMapRadPerSecond =
                    source.angularVelocityRadPerSecond;
                target.angularAccelerationFeedForwardMapRadPerSec2 =
                    glm::dvec3(0.0);
            }

            const double localDuration =
                page.samples[count - 1].timeOffsetSeconds;
            if (!(localDuration > 0.0) ||
                !std::isfinite(localDuration))
            {
                return Result {};
            }

            page.validUntilUniverseTimeSeconds =
                page.acceptedAtUniverseTimeSeconds +
                page.sequenceStartOffsetSeconds +
                localDuration +
                request.policy.validityGraceSeconds;

            page.terminalTolerance.positionMeters =
                request.policy.terminalPositionToleranceMeters;
            page.terminalTolerance.linearVelocityMps =
                request.policy.terminalSpeedToleranceMps;
            page.terminalTolerance.forwardAngleRad =
                request.policy.terminalForwardToleranceRad;
            page.terminalTolerance.angularVelocityRadPerSec =
                request.policy.terminalAngularVelocityToleranceRadPerSec;

            page.tracking.positionErrorMeters =
                request.policy.trackingPositionErrorMeters;
            page.tracking.linearVelocityErrorMps =
                request.policy.trackingLinearVelocityErrorMps;
            page.tracking.forwardAngleErrorRad =
                request.policy.trackingForwardAngleErrorRad;
            page.tracking.angularVelocityErrorRadPerSec =
                request.policy.trackingAngularVelocityErrorRadPerSec;
            page.tracking.alongTrackPositionDeadbandMeters =
                request.policy.alongTrackPositionDeadbandMeters;
            page.tracking.alongTrackSpeedDeadbandMps =
                request.policy.alongTrackSpeedDeadbandMps;
            page.tracking.linearFeedbackReserveMps2 =
                request.policy.linearFeedbackReserveMps2;
            page.tracking.angularFeedbackReserveRadPerSec2 =
                request.policy.angularFeedbackReserveRadPerSec2;

            page.capability = makeManeuverCapabilitySnapshot(
                params,
                request.capabilityRevision
            );

            page.proof.mapRevision = request.mapRevision;
            page.proof.mapSourceRevision = request.mapSourceRevision;
            page.proof.spaceRevision = request.spaceRevision;
            page.proof.spaceSourceRevision = request.spaceSourceRevision;
            page.proof.minimumClearanceMeters =
                request.minimumClearanceMeters;
            page.proof.minimumLinearAuthorityReserveMps2 =
                request.policy.linearFeedbackReserveMps2;
            page.proof.minimumAngularAuthorityReserveRadPerSec2 =
                request.policy.angularFeedbackReserveRadPerSec2;

            page.actuatorSegmentCount =
                static_cast<std::uint8_t>(count - 1);
            page.actuatorProgramFeasible = true;

            for (std::size_t i = 0; i + 1 < count; ++i)
            {
                const auto& a = page.samples[i];
                const auto& b = page.samples[i + 1];
                auto& segment = page.actuatorSegments[i];

                const auto start = compilePropulsion(
                    a,
                    forwardMain,
                    reverseMain,
                    manoeuvre
                );
                const auto finish = compilePropulsion(
                    b,
                    forwardMain,
                    reverseMain,
                    manoeuvre
                );

                segment.durationSeconds =
                    b.timeOffsetSeconds - a.timeOffsetSeconds;
                if (!(segment.durationSeconds > 0.0))
                    return Result {};

                segment.rearMainEnabled =
                    start.rearMainThrottle01 > 1.0e-4 ||
                    finish.rearMainThrottle01 > 1.0e-4;
                segment.rearMainThrottleStart01 =
                    start.rearMainThrottle01;
                segment.rearMainThrottleEnd01 =
                    finish.rearMainThrottle01;

                segment.foreMainEnabled =
                    start.foreMainThrottle01 > 1.0e-4 ||
                    finish.foreMainThrottle01 > 1.0e-4;
                segment.foreMainThrottleStart01 =
                    start.foreMainThrottle01;
                segment.foreMainThrottleEnd01 =
                    finish.foreMainThrottle01;

                segment.manoeuvreAccelerationStartMapMps2 =
                    start.manoeuvreAccelerationMapMps2;
                segment.manoeuvreAccelerationEndMapMps2 =
                    finish.manoeuvreAccelerationMapMps2;
                segment.propulsionFeasible =
                    start.feasible && finish.feasible;
                page.actuatorProgramFeasible =
                    page.actuatorProgramFeasible &&
                    segment.propulsionFeasible;
            }

            if (!page.actuatorProgramFeasible)
                return Result {};

            // Storage pages are not semantic phases. Only the final page may
            // complete the accepted objective.
            page.completionTriggersReplan =
                last + 1 == samples.size();

            out.pages.push_back(std::move(page));

            if (last + 1 >= samples.size())
                break;
            first = last;
        }

        out.valid = !out.pages.empty();
        return out;
    }

private:
    struct PlannedPropulsion
    {
        double rearMainThrottle01 = 0.0;
        double foreMainThrottle01 = 0.0;
        glm::dvec3 manoeuvreAccelerationMapMps2 {0.0};
        bool feasible = true;
    };

    [[nodiscard]] static glm::dquat normalizedOrIdentity(
        const glm::dquat& value
    ) noexcept
    {
        const double n2 =
            value.w * value.w +
            value.x * value.x +
            value.y * value.y +
            value.z * value.z;
        if (!std::isfinite(n2) || n2 <= 1.0e-18)
            return glm::dquat(1.0, 0.0, 0.0, 0.0);
        return value / std::sqrt(n2);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double n2 = glm::dot(value, value);
        if (!std::isfinite(n2) || n2 <= 1.0e-18)
            return fallback;
        return value / std::sqrt(n2);
    }

    [[nodiscard]] static PlannedPropulsion compilePropulsion(
        const AcceptedManeuverProgram::ReferenceSample& sample,
        double forwardMain,
        double reverseMain,
        double manoeuvre
    ) noexcept
    {
        PlannedPropulsion out;
        const glm::dvec3 forward = normalizedOr(
            sample.forwardMap,
            glm::dvec3(0.0, 0.0, -1.0)
        );
        const double requestedForward = glm::dot(
            sample.linearAccelerationFeedForwardMapMps2,
            forward
        );

        const double rear = std::clamp(
            requestedForward,
            0.0,
            std::max(0.0, forwardMain)
        );
        const double fore = std::clamp(
            -requestedForward,
            0.0,
            std::max(0.0, reverseMain)
        );

        out.rearMainThrottle01 =
            forwardMain > 1.0e-9 ? rear / forwardMain : 0.0;
        out.foreMainThrottle01 =
            reverseMain > 1.0e-9 ? fore / reverseMain : 0.0;

        const glm::dvec3 mainAcceleration =
            forward * (rear - fore);
        glm::dvec3 manoeuvreDemand =
            sample.linearAccelerationFeedForwardMapMps2 -
            mainAcceleration;

        const double manoeuvreMagnitude =
            glm::length(manoeuvreDemand);
        out.feasible =
            requestedForward <=
                forwardMain + manoeuvre + 1.0e-6 &&
            requestedForward >=
                -reverseMain - manoeuvre - 1.0e-6 &&
            manoeuvreMagnitude <= manoeuvre + 1.0e-6;

        if (manoeuvreMagnitude > manoeuvre &&
            manoeuvreMagnitude > 1.0e-12)
        {
            manoeuvreDemand *=
                manoeuvre / manoeuvreMagnitude;
        }

        out.manoeuvreAccelerationMapMps2 = manoeuvreDemand;
        return out;
    }
};

} // namespace game::navigation
