#pragma once

// J drive figures (author's jump / J design 2026-11-05, JUMP_MECHANICS.md).
//
// J is the shallow mode of the jump drive's decoupling field: the hull and the
// crew do not feel the drive's acceleration, the motion still follows gravity.
//   - acceleration up to ~1e5 g, so the inner solar system is crossed within an
//     hour (Earth -> Jupiter ~45-65 min);
//   - speed cap 0.9 c against the J reference body (the field collapses above it);
//   - the field does not work near objects: within the keep-out of a hub,
//     station, beacon (KeepOutObjectM) or another ship (KeepOutShipM) the drive
//     is limited to a gentle main-drive-like push (GentleAccelMps2, GentleSpeedMps
//     against that object). The pilot flies out first.
//   - the J floor of a body (author 2026-11-05): below max(top of the atmosphere + 50 km,
//     5 % of the radius, 50 km) the field does not work either - J ends at an orbit /
//     a point above the floor, the descent, landing and the climb back to the floor go
//     on the ship's own main engines (thrust along the hull: tail down). Earth ~320 km,
//     Moon ~87 km, Mars ~170 km;
//   - a landing needs main-engine thrust >= LandingThrustToWeightMin x surface gravity
//     (to land without crashing AND to take off again); a body with an atmosphere takes
//     only shuttles (class surface_lifter: heat shield, landing legs).
// J charge (author 2026-11-06): a full charge is enough for a trip across 2-3 star systems by
// jumps; plain J costs 2-3 times less, by the load of the drive; the ratio scales with the drive.
// The jump rule is the calibrated logistics one (ship_economics.json jump_rule, logistics.py):
//   charge per jump = q * d_ly * (alpha + M_t), q lowered 17 % (6.7e-5 -> 5.6e-5 per ly) so that
//   2 mean jumps + 3 J legs fit the Cobrett's 3 t (3 x std.tank.jumpcharge, 1 t each);
//   a J leg = R x a mean jump, R = 1/3 .. 1/2 by the core load M / rating (light = 3x cheaper);
//   the rate grows with the acceleration: (a / a_max)^1.5 - flying gentler is cheaper.
// Runtime figures are the Cobrett's (the only flown ship): core um.core.z05 rated 500 t, 3 tanks.
#include <algorithm>
#include <cmath>

namespace game::simulation::jdrive
{
constexpr double StandardGravityMps2 = 9.80665;
constexpr double LightSpeedMps = 299792458.0;
constexpr double MaxAccelMps2 = 1.0e5 * StandardGravityMps2;   // ~980 km/s2
constexpr double SpeedCapMps = 0.9 * LightSpeedMps;              // ~269 800 km/s
constexpr double KeepOutObjectM = 20000.0;                       // hubs, stations, beacons
constexpr double KeepOutShipM = 5000.0;                          // other ships
constexpr double GentleAccelMps2 = 9.81;
constexpr double GentleSpeedMps = 300.0;
constexpr double LandingThrustToWeightMin = 1.3;
constexpr double FloorClimbSpeedMps = 400.0;    // main-engine climb out to the J floor

constexpr double ChargePerTankT = 1.0;           // std.tank.jumpcharge
constexpr double CobrettChargeCapacityT = 3.0;   // 3 tanks (passport kb.ship.cobrett-i.*)
constexpr double CobrettCoreRatingT = 500.0;     // um.core.z05
constexpr double JumpChargePerLyT = 5.6e-5;      // q (per ly, per tonne of alpha + M)
constexpr double MeanJumpLy = 22.83;
constexpr double JumpEnvelopeT = 388.0;          // alpha_ref
constexpr double ReferenceLegSeconds = 950.0;    // a mean 1.2 AU leg at a_max with the 0.9 c cap

inline double meanJumpChargeT(double massT)
{
    return JumpChargePerLyT * MeanJumpLy * (JumpEnvelopeT + massT);
}

inline double jLegRatio(double massT, double coreRatingT)
{
    const double load = massT / std::max(1.0, coreRatingT);
    return 1.0 / 3.0 + (1.0 / 6.0) * std::clamp((load - 0.4) / 0.5, 0.0, 1.0);
}

// tonnes of charge per second at the J acceleration `accelMps2`
inline double jChargeRateTps(double accelMps2, double massT, double coreRatingT)
{
    const double x = std::min(1.0, std::abs(accelMps2) / (1.0e5 * StandardGravityMps2));
    return meanJumpChargeT(massT) * jLegRatio(massT, coreRatingT) / ReferenceLegSeconds * std::pow(x, 1.5);
}

inline double floorAltitudeM(double radiusM, double atmosphereRadiusM)
{
    const double atmosphereTop = atmosphereRadiusM > radiusM ? atmosphereRadiusM - radiusM : 0.0;
    return std::max({atmosphereTop + 50000.0, 0.05 * radiusM, 50000.0});
}
}
