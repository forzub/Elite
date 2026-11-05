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
// Not yet: J energy accounting (needs the author's fuel rule).
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
}
