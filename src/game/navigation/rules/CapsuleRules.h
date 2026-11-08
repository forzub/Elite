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

} // namespace game::navigation::capsule
