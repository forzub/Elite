#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation::autopilot
{

// Pure longitudinal speed-envelope calculations.
//
// Responsibility:
//   actual speed + future route speed restriction + physical response
//       -> whether slowdown must begin now and the required preparation range.
//
// It has no route steering, hull orientation or actuator-output authority.
class RouteSpeedGuidance final
{
public:
    struct TurnSlowdownRequest
    {
        double actualSpeedMps = 0.0;
        double turnSpeedCeilingMps = 0.0;
        double distanceToTurnMeters = 0.0;
        double targetSetpointRateMps2 = 0.0;
        double effectiveBrakingMps2 = 0.0;
        double feedbackResponseSeconds = 0.0;
    };

    struct TurnSlowdownResult
    {
        bool valid = false;
        bool slowdownRequiredNow = false;

        double speedDeltaMps = 0.0;
        double setpointSlewSeconds = 0.0;
        double idealBrakeDistanceMeters = 0.0;
        double requiredPreparationDistanceMeters = 0.0;
    };

    [[nodiscard]] static TurnSlowdownResult evaluateTurnSlowdown(
        const TurnSlowdownRequest& request
    ) noexcept
    {
        TurnSlowdownResult out;

        if (!finiteNonNegative(request.actualSpeedMps) ||
            !finiteNonNegative(request.turnSpeedCeilingMps) ||
            !finiteNonNegative(request.distanceToTurnMeters) ||
            !finiteNonNegative(request.targetSetpointRateMps2) ||
            !finiteNonNegative(request.effectiveBrakingMps2) ||
            !finiteNonNegative(request.feedbackResponseSeconds))
        {
            return out;
        }

        out.speedDeltaMps =
            std::max(
                0.0,
                request.actualSpeedMps -
                request.turnSpeedCeilingMps
            );

        out.setpointSlewSeconds =
            request.targetSetpointRateMps2 > 1.0e-9
                ? out.speedDeltaMps /
                    request.targetSetpointRateMps2
                : (out.speedDeltaMps > 1.0e-9
                    ? std::numeric_limits<double>::infinity()
                    : 0.0);

        out.idealBrakeDistanceMeters =
            request.effectiveBrakingMps2 > 1.0e-9 &&
            request.actualSpeedMps > request.turnSpeedCeilingMps
                ? (
                    request.actualSpeedMps * request.actualSpeedMps -
                    request.turnSpeedCeilingMps *
                        request.turnSpeedCeilingMps
                  ) /
                  (2.0 * request.effectiveBrakingMps2)
                : 0.0;

        out.requiredPreparationDistanceMeters =
            request.actualSpeedMps * out.setpointSlewSeconds +
            out.idealBrakeDistanceMeters +
            request.actualSpeedMps *
                request.feedbackResponseSeconds;

        out.slowdownRequiredNow =
            out.speedDeltaMps > 1.0e-9 &&
            out.requiredPreparationDistanceMeters + 1.0e-9 >=
                request.distanceToTurnMeters;

        out.valid =
            std::isfinite(out.speedDeltaMps) &&
            std::isfinite(out.setpointSlewSeconds) &&
            std::isfinite(out.idealBrakeDistanceMeters) &&
            std::isfinite(out.requiredPreparationDistanceMeters);

        return out;
    }

private:
    [[nodiscard]] static bool finiteNonNegative(double value) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    }
};

} // namespace game::navigation::autopilot
