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
// Not yet: J energy accounting (needs the author's fuel rule).
#include <algorithm>

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

inline double floorAltitudeM(double radiusM, double atmosphereRadiusM)
{
    const double atmosphereTop = atmosphereRadiusM > radiusM ? atmosphereRadiusM - radiusM : 0.0;
    return std::max({atmosphereTop + 50000.0, 0.05 * radiusM, 50000.0});
}
}
