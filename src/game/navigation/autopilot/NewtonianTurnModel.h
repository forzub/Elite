#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/navigation/autopilot/NewtonianFlightPolicy.h"

namespace game::navigation::autopilot
{

// How a Newtonian hull turns in planned flight: ONE model for the pilot (its
// calm turns, flip lead) and the route planner (room for a flip before a
// brake). Facts from NavigationControlCapability, shares from
// NewtonianFlightPolicy.

// Calm turn rate: a share of the installed pitch/yaw limit (incl. the
// hull-load bound).
[[nodiscard]] inline double newtonianPreferredTurnRateRadPerSec(
    const NavigationControlCapability& facts) noexcept
{
    const double installed = std::min(
        facts.maxAngularRateRadPerSec.x, facts.maxAngularRateRadPerSec.y);
    return std::isfinite(installed)
        ? NewtonianFlightPolicy::PreferredRateShare * std::max(0.0, installed)
        : 0.0;
}

[[nodiscard]] inline double newtonianPreferredTurnAccelerationRadPerSec2(
    const NavigationControlCapability& facts) noexcept
{
    // Planned turns combine axes (pitch / yaw with the tunnel roll): never
    // plan more of the single-axis limit than two axes can use together
    // (healthy Cobra 0.502 > 0.5: unchanged; asymmetric damage can be less).
    return std::min(NewtonianFlightPolicy::PreferredAccelerationShare,
                    facts.rcsCombinedShare01) *
        std::max(0.0, facts.angularScalarRadPerSec2);
}

// Calm rest-to-rest turn time at the preferred rate/acceleration, plus the
// align/settle time before a burn may start.
[[nodiscard]] inline double newtonianPlannedTurnSeconds(
    double angleRad, const NavigationControlCapability& facts) noexcept
{
    const double angle = std::max(0.0, angleRad);
    const double rate = newtonianPreferredTurnRateRadPerSec(facts);
    const double acceleration = newtonianPreferredTurnAccelerationRadPerSec2(facts);
    if (!(rate > 1.0e-9 && acceleration > 1.0e-9))
        return std::numeric_limits<double>::infinity();
    const double turn =
        angle <= rate * rate / acceleration
            ? 2.0 * std::sqrt(angle / acceleration)
            : angle / rate + rate / acceleration;
    return turn + NewtonianFlightPolicy::AlignSettleSeconds;
}

// Lead before a thrust reversal (a planned 180 degree flip plus margin).
[[nodiscard]] inline double newtonianFlipLeadSeconds(
    const NavigationControlCapability& facts) noexcept
{
    constexpr double Pi = 3.14159265358979323846;
    const double turn = newtonianPlannedTurnSeconds(Pi, facts);
    return std::isfinite(turn)
        ? turn + NewtonianFlightPolicy::FlipLeadMarginSeconds : 0.0;
}

} // namespace game::navigation::autopilot
