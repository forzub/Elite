#pragma once

// THE LAWS OF THIS UNIVERSE (author 2026-11-24: «этот закон надо выносить в глобальные величины описания мира, чтобы
// не было вопросов в дальнейшем. Всё подчиняется местной физике»). One place for the physical constants and laws that
// every moving thing obeys - a ship, a capsule, a drone, a wreck - whatever drives it. Technology (an engine, the J
// field, a hull) lives with its equipment and only USES these.
//
// The medium (author 2026-11-12): space is not a perfect vacuum. Up to MediumOnsetMps a body coasts freely; above it the
// particles it sweeps up resist, on a steep exponential, against the medium's rest frame (the dominant body / the star).
// A thrusting body flies where its thrust equals the resistance - more thrust, a higher top speed, but only
// logarithmically; with the thrust off the medium brakes it back to the onset, and from there it coasts. The thrust that
// only holds the speed is still spent.
//   resistance(v) = MediumDragA0 * (exp((v - onset) / scale) - 1),  v > onset
//   terminal(a)   = onset + scale * ln(1 + a / MediumDragA0)
// Calibrated so that 1e5 g holds 0.9 c: 1 g -> 0.67 c, 100 g -> 0.76 c, 1e3 g -> 0.81 c, 1e4 g -> 0.85 c;
// resistance 0.7 c 4.5 g, 0.75 c 55 g, 0.8 c 670 g, 0.85 c 8 200 g.

#include <algorithm>
#include <cmath>

namespace world::laws
{

inline constexpr double LightSpeedMps = 299792458.0;
inline constexpr double StandardGravityMps2 = 9.80665;

// ---- the medium
inline constexpr double MediumOnsetMps = 0.6 * LightSpeedMps;          // below it a body coasts freely
inline constexpr double MediumScaleMps = 0.02 * LightSpeedMps;         // the exponential's scale
inline constexpr double MediumCalibrationAccelMps2 = 1.0e5 * StandardGravityMps2;   // holds MediumCalibrationSpeedMps
inline constexpr double MediumCalibrationSpeedMps = 0.9 * LightSpeedMps;

// the resistance's base: a_cal / (e^((v_cal - onset) / scale) - 1) = 1e5 g / (e^15 - 1) ~ 0.03 g
inline double mediumDragA0Mps2()
{
    return MediumCalibrationAccelMps2 / (std::exp((MediumCalibrationSpeedMps - MediumOnsetMps) / MediumScaleMps) - 1.0);
}

// the resistance (m/s2, against the motion) at `speedMps` relative to the medium. `speedScale` < 1 when the body's own
// technology meets the medium worse (the J field: a body without a whole field gets every speed of the law halved -
// that rule belongs to the field, jdrive::jFieldFactor)
inline double mediumResistanceMps2(double speedMps, double speedScale = 1.0)
{
    const double onset = MediumOnsetMps * speedScale;
    if (speedMps <= onset)
        return 0.0;
    return mediumDragA0Mps2() * (std::exp((speedMps - onset) / (MediumScaleMps * speedScale)) - 1.0);
}

// the speed a thrust `accelMps2` holds against the medium
inline double mediumTerminalSpeedMps(double accelMps2, double speedScale = 1.0)
{
    return speedScale * (MediumOnsetMps + MediumScaleMps * std::log1p(std::max(0.0, accelMps2) / mediumDragA0Mps2()));
}

} // namespace world::laws
