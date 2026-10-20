#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation::autopilot
{

// The route's time-optimal speed profile is conditional on the actual craft
// remaining able to capture its centreline. A damaged Assisted ship may lose
// that ability on a STRAIGHT even before encountering its first bend.
//
// This velocity governor observes live lateral divergence. It stays entirely
// in the virtual pilot's scalar speed-request path: it cannot change the
// route, insert an RCS burn, or command any physical actuator directly.
// Lower speed restores manoeuvre/turn reserve; full authority still comes
// from the normal installed motors and flight laws.
class AssistedCorridorSpeedGovernor final
{
public:
    struct State
    {
        double recoverySpeedCeilingMps =
            std::numeric_limits<double>::infinity();
    };

    struct Result
    {
        bool valid = false;
        bool recoveryActive = false;
        double speedCeilingMps =
            std::numeric_limits<double>::infinity();
    };

    [[nodiscard]] static Result evaluate(
        State& state,
        double crossTrackMeters,
        double closingSpeedMps,
        double toleranceMeters,
        double actualSpeedMps,
        double nominalTargetSpeedMps,
        double forwardAccelerationAuthorityMps2,
        double deltaSeconds
    ) noexcept
    {
        Result out;
        if (!std::isfinite(crossTrackMeters) ||
            !std::isfinite(closingSpeedMps) ||
            !std::isfinite(toleranceMeters) ||
            !std::isfinite(actualSpeedMps) ||
            !std::isfinite(nominalTargetSpeedMps) ||
            !std::isfinite(forwardAccelerationAuthorityMps2) ||
            !std::isfinite(deltaSeconds) ||
            crossTrackMeters < 0.0 || toleranceMeters <= 0.0 ||
            actualSpeedMps < 0.0 || nominalTargetSpeedMps < 0.0 ||
            forwardAccelerationAuthorityMps2 < 0.0 ||
            deltaSeconds <= 0.0)
        {
            return out;
        }

        out.valid = true;
        const double fraction = std::clamp(
            crossTrackMeters / toleranceMeters, 0.0, 1.0
        );
        // Ignore harmless centering oscillations. Twenty-five percent
        // occupancy is well beyond the known healthy/aft-50 ~8m offset
        // in the 64m acceptance tunnel.
        const bool diverging =
            fraction >= 0.25 && closingSpeedMps < -0.5;
        if (diverging)
        {
            // 15-65% current-speed reduction depending on corridor use.
            // The cap is monotonically nonincreasing while the trajectory
            // continues to diverge, preventing the published 400m/s cruise
            // profile from overriding evidence of an uncatchable course.
            const double occupancy =
                std::clamp((fraction - 0.25) / 0.75, 0.0, 1.0);
            const double safeFraction =
                0.85 - 0.50 * occupancy;
            state.recoverySpeedCeilingMps = std::min(
                state.recoverySpeedCeilingMps,
                actualSpeedMps * safeFraction
            );
        }
        else if (std::isfinite(state.recoverySpeedCeilingMps))
        {
            // Reaccelerate only after outward drift stops. Use real forward
            // authority and elapsed time to prevent a cruise-speed step.
            state.recoverySpeedCeilingMps +=
                forwardAccelerationAuthorityMps2 *
                0.20 * deltaSeconds;
            if (state.recoverySpeedCeilingMps >= nominalTargetSpeedMps)
                state.recoverySpeedCeilingMps =
                    std::numeric_limits<double>::infinity();
        }

        out.speedCeilingMps = state.recoverySpeedCeilingMps;
        out.recoveryActive =
            std::isfinite(out.speedCeilingMps) &&
            out.speedCeilingMps < nominalTargetSpeedMps;
        return out;
    }
};

} // namespace game::navigation::autopilot
