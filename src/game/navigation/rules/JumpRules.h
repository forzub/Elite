#pragma once

// The interstellar jump (author 2026-11-08, JUMP_MECHANICS.md; the plan's points 1-6 confirmed):
//   1. the charge-up takes 5-10 min (longer for a longer hop), silent; the jump itself is instant;
//   2. limits: the charge in the tanks (the calibrated logistics rule, JDrive.h: q * d_ly *
//      (alpha + M_t)), the jump core (a ship heavier than its core's rating cannot jump) and the
//      technology of the time - the longest hop of 3126 is 32 ly (canon world.json max_hop_ly);
//   3. "РАССЧИТАТЬ ПРЫЖОК" checks these before START (a multi-hop planner with refuel points next);
//   6. the exit: at a hub where one is written in, else on an orbit of a planet, else in space
//      (version 1: the orbit of the planet with the most world sites, else the heaviest one).
// Shared by the server (authority) and the client (the calculation shown before START).
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "src/game/navigation/rules/JDrive.h"

namespace game::navigation::jump
{
constexpr double MaxHopLy = 32.0;               // the technology of 3126
constexpr double MinChargeSeconds = 300.0;       // 5 min
constexpr double MaxChargeSeconds = 600.0;       // 10 min
constexpr double ExitOrbitAboveFloorM = 100000.0; // the exit orbit: the body's J floor + 100 km

enum class Refusal : std::uint8_t
{
    None = 0,
    NotInNormalFlight = 1,   // the jump field needs normal flight (no hub / ground stabilization)
    TooFar = 2,              // beyond the longest hop of the time
    CoreOverloaded = 3,      // the ship is heavier than the jump core's rating
    NotEnoughCharge = 4,
    NearObjects = 5,         // charged, waiting: too close to an object / under a J floor
    SameSystem = 6,
    UnknownSystem = 7,
    Cancelled = 8,           // by the pilot
    Interrupted = 9,         // the ship left normal flight during the charge-up
    FieldIncomplete = 10,    // a jump-field emitter (or the core) is lost - repair first (author 2026-11-10)
    ShipDisabled = 11,       // the charge-up ended: the ship is out of the fight (cockpit / all reactors lost)
    ShipDestroyed = 12       // the charge-up ended: the ship was destroyed (impact / dense atmosphere)
};

inline double chargeT(double distanceLy, double massT)
{
    return jdrive::JumpChargePerLyT * std::max(0.0, distanceLy) * (jdrive::JumpEnvelopeT + std::max(0.0, massT));
}

inline double chargeUpSeconds(double distanceLy)
{
    return MinChargeSeconds + (MaxChargeSeconds - MinChargeSeconds) * std::clamp(distanceLy / MaxHopLy, 0.0, 1.0);
}

// the checks before START (the server repeats them; the distance in light years)
inline Refusal check(double distanceLy, double massT, double coreRatingT, double chargeLeftT)
{
    if (distanceLy <= 1.0e-6)
        return Refusal::SameSystem;
    if (distanceLy > MaxHopLy)
        return Refusal::TooFar;
    if (coreRatingT <= 0.0 || massT > coreRatingT)
        return Refusal::CoreOverloaded;
    if (chargeLeftT + 1.0e-9 < chargeT(distanceLy, massT))
        return Refusal::NotEnoughCharge;
    return Refusal::None;
}

// The exit error (tools/colonization_lab/out/world_3126/beacons_3126.json jump_error_model, PROPOSAL):
// sigma_r = d * base_fraction * drive * computer * (1 + age * f_age) * damage / lock - one sigma,
// 3 sigma is the planning box, deterministic as a range. Version 1: standard drive / computer, a
// fresh map, no damage; the beacon lock is approximated by "the system has world sites" (no beacon
// runtime yet): both ends x200, destination only x6, origin only x12, none x1.
constexpr double ExitErrorBaseFraction = 1.0e-9;
constexpr double MetersPerLy = 9.4607304725808e15;
inline double exitLockFactor(bool originBeacon, bool destinationBeacon)
{
    return originBeacon && destinationBeacon ? 200.0 : destinationBeacon ? 6.0 : originBeacon ? 12.0 : 1.0;
}
inline double exitSigmaM(double distanceLy, double lockFactor)
{
    return distanceLy * MetersPerLy * ExitErrorBaseFraction / std::max(1.0, lockFactor);
}
// the exit is "far off" when it lies outside the 3 sigma planning box (a sump later)
constexpr double ExitFarOffSigmas = 3.0;

inline const char* refusalText(Refusal r)
{
    switch (r)
    {
    case Refusal::NotInNormalFlight: return "нужен нормальный полёт (выйдите из стабилизации, Ctrl+K)";
    case Refusal::TooFar: return "дальше 32 св. лет - предел прыжка";
    case Refusal::CoreOverloaded: return "корабль тяжелее номинала прыжкового ядра";
    case Refusal::NotEnoughCharge: return "не хватает заряда прыжкового привода";
    case Refusal::NearObjects: return "заряжено: отойдите от объектов и выше J-порога";
    case Refusal::SameSystem: return "вы уже в этой системе";
    case Refusal::UnknownSystem: return "система не найдена";
    case Refusal::Cancelled: return "прыжок отменён";
    case Refusal::Interrupted: return "зарядка сорвана: корабль вышел из нормального полёта";
    case Refusal::FieldIncomplete: return "поле прыжка неполное: повреждён излучатель поля или ядро - нужен ремонт";
    case Refusal::ShipDisabled: return "зарядка прервана: корабль выведен из строя";
    case Refusal::ShipDestroyed: return "зарядка прервана: корабль разрушен";
    default: return "";
    }
}
} // namespace game::navigation::jump
