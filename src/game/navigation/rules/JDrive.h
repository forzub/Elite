#pragma once

// J drive figures (author's jump / J design 2026-11-05, JUMP_MECHANICS.md).
//
// J is the shallow mode of the jump drive's decoupling field: the hull and the
// crew do not feel the drive's acceleration, the motion still follows gravity.
//   - acceleration up to ~1e5 g, so the inner solar system is crossed within an
//     hour (Earth -> Jupiter ~45-65 min);
//   - no fixed speed cap: free coasting up to 0.6 c, above it the medium resists on a steep exponential (against the
//     dominant body's frame), the full thrust holds ~0.9 c (author 2026-11-12, see mediumResistanceMps2);
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
// The ship's own figures (core rating, charge capacity) come from its descriptor (ShipParams
// jumpCoreRatingT / jumpChargeCapacityT; the Cobrett: um.core.z05 500 t, 3 x 1 t tanks) - no ship
// constants here (architecture audit 2026-11-06, item 4).
#include <algorithm>

#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/world/WorldLaws.h"
#include <cmath>

namespace game::navigation::jdrive
{
// the universe's constants and the medium law (src/world/WorldLaws.h - every moving body obeys them, not only J)
constexpr double StandardGravityMps2 = world::laws::StandardGravityMps2;
constexpr double LightSpeedMps = world::laws::LightSpeedMps;
// the J field's technology limit (~980 km/s2); the medium holds it at 0.9 c (the law's calibration point)
constexpr double MaxAccelMps2 = 1.0e5 * StandardGravityMps2;
constexpr double SpeedCapMps = world::laws::MediumCalibrationSpeedMps;   // the terminal speed at MaxAccelMps2
constexpr double MediumOnsetMps = world::laws::MediumOnsetMps;
constexpr double MediumScaleMps = world::laws::MediumScaleMps;
using world::laws::mediumDragA0Mps2;
using world::laws::mediumResistanceMps2;
using world::laws::mediumTerminalSpeedMps;
constexpr double KeepOutObjectM = 20000.0;                       // hubs, stations, beacons
constexpr double KeepOutShipM = 5000.0;                          // other ships
constexpr double GentleAccelMps2 = 9.81;
constexpr double GentleSpeedMps = 300.0;
constexpr double LandingThrustToWeightMin = 1.3;
constexpr double FloorClimbSpeedMps = 400.0;    // main-engine climb out to the J floor
// the J drive's ramp (1 g, doubling every DriveRampDoublingS) and the arrival tail (d / ApproachTauS),
// shared by the J autopilot and the route legs
constexpr double DriveRampDoublingS = 2.0;
constexpr double ApproachTauS = 6.0;

constexpr double ChargePerTankT = 1.0;           // std.tank.jumpcharge
constexpr double JumpChargePerLyT = 5.6e-5;      // q (per ly, per tonne of alpha + M)
constexpr double MeanJumpLy = 22.83;
constexpr double JumpEnvelopeT = 388.0;          // alpha_ref
constexpr double ReferenceLegSeconds = 950.0;    // a mean 1.2 AU leg at a_max with the 0.9 c cap

// The field emitters (author 2026-11-10, ACCEPTED - docs/SHIP_ARCHITECTURE_DOCTRINE.md, JUMP_MECHANICS.md): the
// jump core makes the field, the emitters on the hull extremities shape it around the hull. One emitter lost: the
// field is asymmetric - the J ceiling (speed cap and acceleration) is halved and a jump is impossible until it is
// repaired; the core or every emitter lost: no field, no J. A hull without declared emitters keeps the full field.
struct JFieldIntegrity
{
    int emittersInstalled = 0;
    int emittersIntact = 0;
    bool coreIntact = true;
};

inline double jFieldFactor(const JFieldIntegrity& j)
{
    if (!j.coreIntact)
        return 0.0;
    if (j.emittersInstalled <= 0 || j.emittersIntact >= j.emittersInstalled)
        return 1.0;
    return j.emittersIntact <= 0 ? 0.0 : 0.5;
}

inline bool jumpFieldComplete(const JFieldIntegrity& j)
{
    return jFieldFactor(j) >= 1.0;
}

inline double meanJumpChargeT(double massT)
{
    return JumpChargePerLyT * MeanJumpLy * (JumpEnvelopeT + massT);
}

inline double jLegRatio(double massT, double coreRatingT)
{
    const double load = massT / std::max(1.0, coreRatingT);
    return 1.0 / 3.0 + (1.0 / 6.0) * std::clamp((load - 0.4) / 0.5, 0.0, 1.0);
}

// J is much cheaper than a jump (author 2026-11-12): with the medium the field pays for holding the speed, and
// Earth -> Jupiter at the full 1e5 g cost ~1 mean jump; x0.1 makes it ~0.1 (an in-system jump there ~0.7)
constexpr double JChargeScale = 0.1;

// tonnes of charge per second at the J acceleration `accelMps2`
inline double jChargeRateTps(double accelMps2, double massT, double coreRatingT)
{
    const double x = std::min(1.0, std::abs(accelMps2) / (1.0e5 * StandardGravityMps2));
    return JChargeScale * meanJumpChargeT(massT) * jLegRatio(massT, coreRatingT) / ReferenceLegSeconds * std::pow(x, 1.5);
}

inline double floorAltitudeM(double radiusM, double atmosphereRadiusM)
{
    return game::navigation::rules::floorAltitudeM(radiusM, atmosphereRadiusM);
}
}
