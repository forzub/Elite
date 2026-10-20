#pragma once

#include <algorithm>
#include <cmath>

namespace game::navigation::autopilot
{

// Active route tracking is NOT proof that the craft can recapture the tunnel.
// A failed asymmetric engine can saturate the simultaneous motor wrench
// despite valid isolated signed-axis telemetry. Stop issuing route-speed
// commands before the virtual pilot flies kilometres from the corridor.
//
// This decision is a hard safety escalation only. It does not replan, alter
// the authored route, estimate thrust, or bypass physical pilot controls.
// 'closingSpeed' is positive TOWARD the centre, negative OUTWARD.
class AssistedCorridorDeviationPolicy final
{
public:
    struct Decision
    {
        bool requireSafeStop = false;
        bool fullToleranceExceeded = false;
        bool divergingNearBoundary = false;
    };

    [[nodiscard]] static Decision evaluate(
        double crossTrackMeters,
        double closingSpeedMps,
        double trackingToleranceMeters
    ) noexcept
    {
        Decision out;
        if (!std::isfinite(crossTrackMeters) ||
            !std::isfinite(closingSpeedMps) ||
            !std::isfinite(trackingToleranceMeters) ||
            crossTrackMeters < 0.0 ||
            trackingToleranceMeters <= 0.0)
        {
            // Invalid observability is NOT permission to keep accelerating.
            out.requireSafeStop = true;
            return out;
        }

        out.fullToleranceExceeded =
            crossTrackMeters >= trackingToleranceMeters;
        // Start braking before the error reaches the safety boundary when
        // the ship is still clearly moving away from the centreline.
        // The half-tolerance and 2 m/s outward drift are conservative
        // diagnostics; a future coupled-wrench predictor can supersede them.
        out.divergingNearBoundary =
            crossTrackMeters >= trackingToleranceMeters * 0.5 &&
            closingSpeedMps <= -2.0;
        out.requireSafeStop =
            out.fullToleranceExceeded ||
            out.divergingNearBoundary;
        return out;
    }
};

} // namespace game::navigation::autopilot
