#pragma once

// The cockpit escape capsule at a fatal impact (author 2026-11-08): on small frontier ships the pilot's
// cockpit IS the escape capsule - "не хрупкая, но разрушаемая". It survives what destroys the ship,
// within limits:
//   - the shell: a surface impact faster than StructureLimitMps crushes it;
//   - the atmosphere: it does not burn up in an Earth-like or thinner atmosphere (surface pressure up
//     to BurnPressureLimitPa); a denser one (Venus) burns / crushes it;
//   - the pilot inside: the body survives a deceleration up to BodyLimitG (restrained, short pulse;
//     J. P. Stapp's sled run peaked at 46 g). A surface impact decelerates the body over the hull's
//     crumple plus the capsule's absorber (CrushStrokeM); an atmospheric breakup decelerates it by
//     the drag, which keeps rising toward the ballistic-entry peak while the capsule still descends.
// Only a ship whose descriptor has the detachable "ship_cockpit" module carries one (Cobrett-I today).

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::capsule
{

inline constexpr double BodyLimitG = 50.0;
inline constexpr double CrushStrokeM = 6.0;
inline constexpr double StructureLimitMps = 1000.0;
inline constexpr double EarthSurfacePressurePa = 101325.0;
inline constexpr double BurnPressureLimitPa = 1.5 * EarthSurfacePressurePa;

// DynamicMotionState::capsuleState
enum class State : std::uint8_t
{
    None = 0,         // the ship is intact (no event)
    PilotAlive = 1,   // the capsule came through, the pilot is alive
    PilotKilled = 2,  // the capsule came through, the body did not (deceleration beyond BodyLimitG)
    Destroyed = 3,    // the capsule itself was destroyed
    NoCapsule = 4     // this ship carries no escape capsule
};

struct Outcome
{
    State state = State::PilotAlive;
    double bodyG = 0.0;   // the deceleration the pilot took
};

// a surface impact: vertical / total speed against the ground (m/s)
inline Outcome surfaceImpact(double verticalMps, double totalMps)
{
    Outcome o;
    o.bodyG = std::max(0.0, verticalMps) * std::max(0.0, verticalMps) / (2.0 * CrushStrokeM) / 9.81;
    if (totalMps > StructureLimitMps)
        o.state = State::Destroyed;
    else if (o.bodyG > BodyLimitG)
        o.state = State::PilotKilled;
    return o;
}

// the dense-atmosphere breakup: the drag now (m/s2), the speed (m/s), the sine of the descent angle
// (> 0 descending), the scale height (m) and the body's surface pressure (Pa)
inline Outcome atmosphereBreakup(double dragMps2, double speedMps, double sinDescent, double scaleHeightM, double surfacePressurePa)
{
    Outcome o;
    // the peak of a ballistic entry (Allen-Eggers): a = v^2 sin(gamma) / (2 e H); still ahead while descending
    const double peak = sinDescent > 0.0 && scaleHeightM > 0.0
        ? speedMps * speedMps * sinDescent / (2.0 * std::exp(1.0) * scaleHeightM) : 0.0;
    o.bodyG = std::max(dragMps2, peak) / 9.81;
    if (surfacePressurePa > BurnPressureLimitPa)
        o.state = State::Destroyed;
    else if (o.bodyG > BodyLimitG)
        o.state = State::PilotKilled;
    return o;
}

inline const char* stateText(State s)
{
    switch (s)
    {
    case State::PilotAlive: return "капсула цела, пилот жив";
    case State::PilotKilled: return "капсула цела, пилот погиб от перегрузки";
    case State::Destroyed: return "капсула разрушена";
    case State::NoCapsule: return "спасательной капсулы нет";
    default: return "";
    }
}

// ---- the COCKPIT-CAPSULE flying (author 2026-11-24, memory cockpit-capsule-rescue): the Cobrett EC-2 cockpit is a small
// ship - 6.5 t, two field-thruster rings aft (catalog kb.surv.capsule2: escape_thrust_mn 0.13 -> 20 m/s2, ~2 g), cold-gas
// attitude jets, a LIMITED reaction mass (escape_dv_mps 3000: spent on the acceleration and the braking, the coast between
// is free). The same figures on the server (the physics) and the client (the autopilot's envelope).
inline constexpr double CapsuleMassKg = 6500.0;
inline constexpr double CapsuleThrustN = 0.13e6;
inline constexpr double CapsuleDeltaVMps = 3000.0;
inline constexpr double CapsuleAngularAccelRadPerSec2 = 0.6;   // the jets stop a 4 rad/s tumble in ~7 s
inline constexpr double CapsuleLateralAccelMps2 = 2.0;         // the jets' translation (all six directions)

// the capsule mode of a ship: its hull lost, the pilot alive in the cockpit (DynamicMotionState destroyedCause +
// capsuleState) - the one test for both sides
inline bool capsuleFlying(std::uint8_t destroyedCause, std::uint8_t capsuleState)
{
    return destroyedCause != 0 && capsuleState == static_cast<std::uint8_t>(State::PilotAlive);
}

// the flight parameters of the capsule (the reaction mass spent -> no main thrust; the jets keep the attitude)
inline ShipParams capsuleParams(double deltaVLeftMps)
{
    ShipParams p {};
    p.maxPitchRate = 1.2f;
    p.maxYawRate = 1.2f;
    p.maxRollRate = 1.5f;
    p.angularAccel = static_cast<float>(CapsuleAngularAccelRadPerSec2);
    p.angularDamping = 2.0f;
    p.maxCombatSpeed = 300.0f;
    p.maxCruiseSpeed = 3000.0f;
    p.throttleAccel = 1.0f;
    p.autoLevelStrength = 0.0f;
    const bool fuel = deltaVLeftMps > 0.0;
    p.forwardMainEngineAvailable = fuel;
    p.forwardMainEngineAccelerationMps2 = fuel ? static_cast<float>(CapsuleThrustN / CapsuleMassKg) : 0.0f;
    p.reverseMainEngineAvailable = false;
    p.strafeAccel = fuel ? static_cast<float>(CapsuleLateralAccelMps2) : 0.0f;
    p.strafeDamping = 1.0f;
    p.maxStrafeSpeed = 50.0f;
    p.manoeuvreThrusterAuthorityExplicit = true;
    p.manoeuvreThrusterAccel = fuel ? static_cast<float>(CapsuleLateralAccelMps2) : 0.0f;
    p.maxGs = 3.0f;
    p.maxLinearGs = 3.0f;
    p.massKg = CapsuleMassKg;
    // a 2.2 x 1.3 x 1.6 m half-size box of 6.5 t
    p.pitchInertiaKgM2 = CapsuleMassKg * (1.3 * 1.3 + 1.6 * 1.6) * 4.0 / 12.0;
    p.yawInertiaKgM2 = CapsuleMassKg * (2.2 * 2.2 + 1.6 * 1.6) * 4.0 / 12.0;
    p.rollInertiaKgM2 = CapsuleMassKg * (2.2 * 2.2 + 1.3 * 1.3) * 4.0 / 12.0;
    p.physicalActuatorHalfExtentsMeters = glm::dvec3(2.2, 1.3, 1.6);
    return p;
}

} // namespace game::navigation::capsule
