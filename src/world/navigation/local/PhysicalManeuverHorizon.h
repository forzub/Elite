#pragma once

#include <algorithm>
#include <cmath>

namespace world::navigation
{

// One physical definition of "how far / how long Navigation must be able to
// see before the current state becomes unrecoverable".
//
// It is deliberately independent of craft identity and obstacle representation.
// Callers provide the currently available braking authority and command-response
// reserve. The same result is consumed by local planning and execution safety.
class PhysicalManeuverHorizon final
{
public:
    struct Query
    {
        double speedMetersPerSecond = 0.0;
        double accelerationMagnitudeMetersPerSecond2 = 0.0;

        // Time already lost to snapshot age plus time that may elapse before a
        // newly selected command materially reaches propulsion.
        double snapshotAgeSeconds = 0.0;
        double controlResponseReserveSeconds = 0.0;

        double brakingAccelerationMetersPerSecond2 = 0.0;

        // Space needed for a non-stop avoidance maneuver after the guaranteed
        // response/braking reserve.
        double turnDistanceMeters = 0.0;
        double safetyMarginMeters = 0.0;
        double minimumDistanceMeters = 0.0;

        // Dynamic hazards require a temporal horizon as well as a distance
        // horizon. This is the ordinary minimum, not a hard cap.
        double minimumLookAheadSeconds = 0.0;
    };

    struct Result
    {
        bool valid = false;

        double responseSeconds = 0.0;
        double speedAtBrakeMetersPerSecond = 0.0;
        double responseDistanceMeters = 0.0;

        double brakingSeconds = 0.0;
        double brakingDistanceMeters = 0.0;

        double distanceMeters = 0.0;
        double lookAheadSeconds = 0.0;
    };

    [[nodiscard]] static Result evaluate(
        const Query& query
    ) noexcept
    {
        Result result;

        if (!finiteNonNegative(query.speedMetersPerSecond) ||
            !finiteNonNegative(
                query.accelerationMagnitudeMetersPerSecond2
            ) ||
            !finiteNonNegative(query.snapshotAgeSeconds) ||
            !finiteNonNegative(query.controlResponseReserveSeconds) ||
            !std::isfinite(query.brakingAccelerationMetersPerSecond2) ||
            query.brakingAccelerationMetersPerSecond2 <= 0.0 ||
            !finiteNonNegative(query.turnDistanceMeters) ||
            !finiteNonNegative(query.safetyMarginMeters) ||
            !finiteNonNegative(query.minimumDistanceMeters) ||
            !finiteNonNegative(query.minimumLookAheadSeconds))
        {
            return result;
        }

        result.responseSeconds =
            query.snapshotAgeSeconds +
            query.controlResponseReserveSeconds;

        // Conservative scalar upper bound: if current acceleration continues
        // during the response window, assume it can increase closing speed.
        result.speedAtBrakeMetersPerSecond =
            query.speedMetersPerSecond +
            query.accelerationMagnitudeMetersPerSecond2 *
                result.responseSeconds;

        result.responseDistanceMeters =
            query.speedMetersPerSecond * result.responseSeconds +
            0.5 *
                query.accelerationMagnitudeMetersPerSecond2 *
                result.responseSeconds *
                result.responseSeconds;

        result.brakingSeconds =
            result.speedAtBrakeMetersPerSecond /
            query.brakingAccelerationMetersPerSecond2;

        result.brakingDistanceMeters =
            result.speedAtBrakeMetersPerSecond *
            result.speedAtBrakeMetersPerSecond /
            (2.0 * query.brakingAccelerationMetersPerSecond2);

        result.distanceMeters = std::max(
            query.minimumDistanceMeters,
            result.responseDistanceMeters +
                result.brakingDistanceMeters +
                query.turnDistanceMeters +
                query.safetyMarginMeters
        );

        result.lookAheadSeconds = std::max(
            query.minimumLookAheadSeconds,
            result.responseSeconds + result.brakingSeconds
        );

        result.valid =
            std::isfinite(result.distanceMeters) &&
            std::isfinite(result.lookAheadSeconds);
        return result;
    }

private:
    [[nodiscard]] static bool finiteNonNegative(
        double value
    ) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    }
};

} // namespace world::navigation
