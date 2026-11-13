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

#include <glm/glm.hpp>

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
    ShipDestroyed = 12,      // the charge-up ended: the ship was destroyed (impact / dense atmosphere)
    StarInTheWay = 13,       // an in-system jump whose straight step crosses the star's capture sphere
    TooClose = 14,           // an in-system jump shorter than InSystemMinAu: fly it in J
    UnknownBody = 15,        // an in-system jump to a body this system does not have
    ZoneViolated = 16,       // during the charge-up an object entered the start zone / the nose left the line: lost
    LineBlocked = 17,        // the straight step is blocked by a body / the star (no way around found)
    SumpCaught = 18          // the step fell into a sump (asteroid / debris swarm, JumpSumps.h): out at the catch point
};

// ---- the jump procedure (author 2026-11-12, JUMP_MECHANICS.md 10b): a jump is an instant move along a straight line;
// the planner takes the ship to a safe start zone (in J; under a J floor on the main engines), aims the nose at the
// step's end, charges the core there and jumps 1 s after the charge is full. An object in the zone, a blocked line or
// the nose off the line during the charge-up aborts it: the core discharges, the charge drawn is lost.
constexpr double AlignToleranceRad = 0.5 * 3.14159265358979323846 / 180.0;
constexpr double ChargeNoseToleranceRad = 3.0 * 3.14159265358979323846 / 180.0;
constexpr double JumpDelayAfterChargeS = 1.0;
constexpr double ZoneAboveFloorM = 300000.0;          // the start zone over a body: its J floor + 300 km
constexpr double ZoneArrivalM = 2000.0;
constexpr double ZoneArrivalSpeedMps = 100.0;
constexpr double ZoneTransitAccelMps2 = 1000.0 * jdrive::StandardGravityMps2;
constexpr double ZoneHoldAccelMps2 = 100.0 * jdrive::StandardGravityMps2;
constexpr double LineCheckPeriodS = 1.0;

// ---- the in-system jump (author 2026-11-05 decision 3, implemented 2026-11-12: "inside a system one can and should
// jump, but dearly - the payment is the charge and the risk where there are no beacons"; Pluto in J is ~8 h):
//   charge   = q * (alpha + M) * (InSystemBaseLy + InSystemLyPerAu * d_AU): half a mean jump to tear the field at all,
//              +5 % of a mean jump per AU (Earth -> Jupiter ~0.7, Earth -> Pluto ~2.5 mean jumps);
//   charge-up 2..5 min by the distance (up to 40 AU);
//   the exit error: InSystemErrorFraction of the distance (one sigma) / the beacon lock (no beacon: 4 AU -> ~600 000 km;
//              both ends on beacons x200 -> ~3 000 km);
//   the star catches the step (decision 8): a straight step through its capture sphere (StarCaptureAu x sqrt(M/Msun))
//              is refused - jump around it through an intermediate point; shorter than InSystemMinAu: fly in J.
constexpr double AuM = 1.495978707e11;
constexpr double InSystemBaseLy = 0.5 * jdrive::MeanJumpLy;
constexpr double InSystemLyPerAu = 0.05 * jdrive::MeanJumpLy;
constexpr double InSystemMinAu = 0.05;
constexpr double InSystemMinChargeSeconds = 120.0;
constexpr double InSystemMaxChargeSeconds = 300.0;
constexpr double InSystemFullChargeAu = 40.0;
constexpr double InSystemErrorFraction = 1.0e-3;
constexpr double StarCaptureAu = 0.3;
constexpr double SunMuM3s2 = 1.32712440018e20;

inline double inSystemChargeT(double distanceAu, double massT)
{
    return jdrive::JumpChargePerLyT * (InSystemBaseLy + InSystemLyPerAu * std::max(0.0, distanceAu)) *
           (jdrive::JumpEnvelopeT + std::max(0.0, massT));
}

inline double inSystemChargeUpSeconds(double distanceAu)
{
    return InSystemMinChargeSeconds +
           (InSystemMaxChargeSeconds - InSystemMinChargeSeconds) * std::clamp(distanceAu / InSystemFullChargeAu, 0.0, 1.0);
}

inline double inSystemExitSigmaM(double distanceAu, double lockFactor)
{
    return distanceAu * AuM * InSystemErrorFraction / std::max(1.0, lockFactor);
}

inline double starCaptureRadiusM(double starMuM3s2)
{
    return StarCaptureAu * AuM * std::sqrt(std::max(0.0, starMuM3s2) / SunMuM3s2);
}

// the closest approach of the segment a -> b to the point c (metres)
inline double segmentDistanceM(const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& c)
{
    const glm::dvec3 ab = b - a;
    const double len2 = glm::dot(ab, ab);
    const double t = len2 > 0.0 ? std::clamp(glm::dot(c - a, ab) / len2, 0.0, 1.0) : 0.0;
    return glm::length(a + ab * t - c);
}

// the in-system checks before START (the server repeats them); the star's position / mu for the capture rule
inline Refusal checkInSystem(const glm::dvec3& fromM, const glm::dvec3& toM, const glm::dvec3& starM, double starMu,
                             double massT, double coreRatingT, double chargeLeftT)
{
    const double d = glm::length(toM - fromM) / AuM;
    if (d < InSystemMinAu)
        return Refusal::TooClose;
    if (coreRatingT <= 0.0 || massT > coreRatingT)
        return Refusal::CoreOverloaded;
    if (segmentDistanceM(fromM, toM, starM) < starCaptureRadiusM(starMu))
        return Refusal::StarInTheWay;
    if (chargeLeftT + 1.0e-9 < inSystemChargeT(d, massT))
        return Refusal::NotEnoughCharge;
    return Refusal::None;
}

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
    case Refusal::StarInTheWay: return "прямой шаг проходит через зону захвата звезды - прыгайте через промежуточную точку";
    case Refusal::TooClose: return "цель ближе 0.05 а.е. - туда летят на J";
    case Refusal::UnknownBody: return "тело не найдено в системе";
    case Refusal::ZoneViolated: return "срыв: в зону старта вошла помеха или нос ушёл с линии - ядро разряжено, заряд потерян";
    case Refusal::LineBlocked: return "прямая к цели перекрыта телом - обхода не найдено";
    case Refusal::SumpCaught: return "прыжок захвачен отстойником - корабль вышел раньше цели, заряд шага потрачен";
    default: return "";
    }
}
} // namespace game::navigation::jump
