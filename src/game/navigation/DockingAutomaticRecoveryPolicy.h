#pragma once

#include <algorithm>
#include <cmath>

namespace game::navigation
{

// Automatic docking recovery is deliberately a game-flight policy, not a
// second planner. It decides how much real authority the trajectory must leave
// unused for Follower correction and which small dynamic errors may be corrected
// in place without throwing away a still-safe spatial route.
class DockingAutomaticRecoveryPolicy final
{
public:
    static double linearFeedbackReserveMps2(
        double forwardAuthorityMps2,
        double brakingAuthorityMps2,
        double lateralAuthorityMps2,
        double reserveFraction = 0.20
    ) noexcept
    {
        if (!std::isfinite(forwardAuthorityMps2) ||
            !std::isfinite(brakingAuthorityMps2) ||
            !std::isfinite(lateralAuthorityMps2) ||
            !std::isfinite(reserveFraction))
        {
            return 0.0;
        }

        const double weakestAuthority = std::max(
            0.0,
            std::min({
                forwardAuthorityMps2,
                brakingAuthorityMps2,
                lateralAuthorityMps2
            })
        );
        return weakestAuthority *
            std::clamp(reserveFraction, 0.0, 1.0);
    }

    // Stage 1 has already proved the HOLD point spatially safe. If the craft is
    // inside the accepted tracking-position envelope and slow enough to be
    // settled by Stage 2 stabilization, do not launch another long approach.
    static double holdCaptureDistanceMeters(
        double trackingPositionErrorMeters
    ) noexcept
    {
        if (!std::isfinite(trackingPositionErrorMeters))
            return 0.0;
        return std::max(12.0, trackingPositionErrorMeters);
    }

    // HOLD capture is a braking problem, not a magic speed threshold.
    // If the craft is already inside the accepted spatial envelope and can
    // physically stop before leaving it, transition to FinalIngress and let
    // the dedicated stop/alignment phase finish the job.
    static bool canCaptureHoldWhileBraking(
        double distanceToHoldMeters,
        double trackingPositionErrorMeters,
        double speedMps,
        double brakingAuthorityMps2,
        double responseSeconds = 0.25
    ) noexcept
    {
        const double values[] = {
            distanceToHoldMeters,
            trackingPositionErrorMeters,
            speedMps,
            brakingAuthorityMps2,
            responseSeconds
        };
        for (double value : values)
        {
            if (!std::isfinite(value) || value < 0.0)
                return false;
        }

        if (!(brakingAuthorityMps2 > 0.0))
            return false;

        const double captureRadius =
            holdCaptureDistanceMeters(
                trackingPositionErrorMeters
            );
        if (!(captureRadius > 0.0) ||
            distanceToHoldMeters > captureRadius)
        {
            return false;
        }

        const double availableBrakingDistance =
            std::max(
                0.0,
                captureRadius - distanceToHoldMeters
            );
        const double requiredBrakingDistance =
            speedMps * responseSeconds +
            (speedMps * speedMps) /
                (2.0 * brakingAuthorityMps2);

        return requiredBrakingDistance <=
            availableBrakingDistance + 1.0e-9;
    }

    // A route is still geometrically safe while position and hull direction are
    // inside their proved envelope. A modest velocity/rate mismatch is then a
    // control problem: let bounded feedback remove it instead of stopping,
    // replanning and potentially sending the ship backwards along a new route.
    static bool recoverableDynamicExcursion(
        double positionErrorMeters,
        double positionLimitMeters,
        double velocityErrorMps,
        double velocityLimitMps,
        double forwardAngleErrorRad,
        double forwardAngleLimitRad,
        double angularVelocityErrorRadPerSec,
        double angularVelocityLimitRadPerSec,
        double actualAngularSpeedRadPerSec,
        double capabilityAngularSpeedRadPerSec
    ) noexcept
    {
        const double values[] = {
            positionErrorMeters,
            positionLimitMeters,
            velocityErrorMps,
            velocityLimitMps,
            forwardAngleErrorRad,
            forwardAngleLimitRad,
            angularVelocityErrorRadPerSec,
            angularVelocityLimitRadPerSec,
            actualAngularSpeedRadPerSec,
            capabilityAngularSpeedRadPerSec
        };
        for (double value : values)
        {
            if (!std::isfinite(value) || value < 0.0)
                return false;
        }

        if (positionLimitMeters <= 0.0 ||
            velocityLimitMps <= 0.0 ||
            forwardAngleLimitRad <= 0.0 ||
            angularVelocityLimitRadPerSec <= 0.0 ||
            capabilityAngularSpeedRadPerSec <= 0.0)
        {
            return false;
        }

        return
            positionErrorMeters <= positionLimitMeters &&
            forwardAngleErrorRad <= forwardAngleLimitRad &&
            velocityErrorMps <= 1.25 * velocityLimitMps &&
            angularVelocityErrorRadPerSec <=
                2.0 * angularVelocityLimitRadPerSec &&
            actualAngularSpeedRadPerSec <=
                capabilityAngularSpeedRadPerSec;
    }
};

} // namespace game::navigation
