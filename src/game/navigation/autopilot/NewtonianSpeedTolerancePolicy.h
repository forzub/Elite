#pragma once

#include <cmath>

namespace game::navigation::autopilot
{

// Newtonian speed tolerance corridor (one owner).
//
// A profile speed is the NOMINAL controlled speed of ordinary flight, not a
// floating-point boundary: speeds inside the soft band around it are the same
// strategic state. Inside the band no manoeuvre is planned for the
// difference: the runtime profile is not re-anchored on it (no brake/burn
// segment, no speed valley), the manoeuvre program is not recompiled for it
// and the executor does not chase it. Only a deviation beyond the band is a
// real over/underspeed; its correction is entered beyond the band and left
// at half of it (hysteresis), and it is a bounded profile correction, not an
// emergency. The physical fail-safe (propulsion never increases a speed above
// the ship's controlled speed limit) is a separate layer and unchanged.
//
// The band scales with the speed: a few m/s at a 400 m/s transit, below one
// m/s at a narrow-zone or curve speed of tens of m/s. Terminal (route stop)
// and future capture speeds use their own geometric tolerances, not this
// band. The numbers equal the executor's long-standing longitudinal
// deadband, so moving them here changes no control law.
struct NewtonianSpeedTolerancePolicy
{
    static constexpr double FloorMps = 0.5;
    static constexpr double SpeedShare = 0.005;
    static constexpr double ReleaseShare = 0.5;

    // Soft band half-width around a nominal speed.
    [[nodiscard]] static double softBandMps(double nominalSpeedMps) noexcept
    {
        const double v =
            std::isfinite(nominalSpeedMps) ? std::abs(nominalSpeedMps) : 0.0;
        return FloorMps + SpeedShare * v;
    }

    // A craft off the profile it is flying by more than this at a route
    // checkpoint (a re-levelled plateau, a tracked ramp, a real deviation)
    // gets its manoeuvre program refreshed; a craft on it does not.
    static constexpr double ProgramRefreshShare = 0.1;
    [[nodiscard]] static double programRefreshMps(double speedMps) noexcept
    {
        return ProgramRefreshShare * softBandMps(speedMps);
    }

    // A profile segment whose longitudinal acceleration stays within this is
    // a speed plateau (cruise, curve or narrow-zone speed).
    static constexpr double PlateauAccelerationMps2 = 1.0e-3;

    // Real deviation (correction entered) / correction finished.
    [[nodiscard]] static bool beyondBand(
        double actualSpeedMps, double nominalSpeedMps) noexcept
    {
        return std::abs(actualSpeedMps - nominalSpeedMps) >
            softBandMps(nominalSpeedMps);
    }
    [[nodiscard]] static bool backInsideRelease(
        double actualSpeedMps, double nominalSpeedMps) noexcept
    {
        return std::abs(actualSpeedMps - nominalSpeedMps) <=
            ReleaseShare * softBandMps(nominalSpeedMps);
    }
};

} // namespace game::navigation::autopilot
