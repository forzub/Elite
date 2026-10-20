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
        double trackingToleranceMeters,
        double responseSeconds,
        double availableLateralAccelerationMps2
    ) noexcept
    {
        Decision out;
        if (!std::isfinite(crossTrackMeters) ||
            !std::isfinite(closingSpeedMps) ||
            !std::isfinite(trackingToleranceMeters) ||
            !std::isfinite(responseSeconds) ||
            !std::isfinite(availableLateralAccelerationMps2) ||
            crossTrackMeters < 0.0 ||
            trackingToleranceMeters <= 0.0 ||
            responseSeconds <= 0.0 ||
            availableLateralAccelerationMps2 <= 0.0)
        {
            // Invalid observability is NOT permission to keep accelerating.
            out.requireSafeStop = true;
            return out;
        }

        out.fullToleranceExceeded =
            crossTrackMeters >= trackingToleranceMeters;
        // There is no justification for abandoning the visible route at
        // 32m simply because it is moving outward at 2m/s: the craft may
        // still have enough room and lateral authority to capture it.
        //
        // Escalate inside the tunnel only when predicted outward drift during
        // the measured control response PLUS ideal lateral stopping distance
        // consumes the remaining corridor. This is still a conservative
        // lower-bound estimate, not a claim of coupled-wrench feasibility.
        const double outwardSpeed = std::max(0.0, -closingSpeedMps);
        const double stoppingDistance =
            outwardSpeed * outwardSpeed /
            (2.0 * availableLateralAccelerationMps2);
        const double predictedCrossTrack =
            crossTrackMeters +
            outwardSpeed * responseSeconds +
            stoppingDistance;
        out.divergingNearBoundary =
            outwardSpeed > 0.5 &&
            predictedCrossTrack >= trackingToleranceMeters;
        out.requireSafeStop =
            out.fullToleranceExceeded ||
            out.divergingNearBoundary;
        return out;
    }
};

} // namespace game::navigation::autopilot
