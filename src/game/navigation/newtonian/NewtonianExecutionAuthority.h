#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::newtonian
{

// Newtonian route-execution authority.
//
// A Newtonian craft does NOT have a persistent body-lateral route-thrust
// channel. Route acceleration in any direction is produced by rotating the
// hull/main-thrust axis and then burning the aft main engine. Main-nozzle TVC
// reduces the residual hull angle but does not create a second acceleration
// budget. Small manoeuvre/RCS thrusters are deliberately absent from this
// authority: they are attitude/precision actuators, not sustained route thrust.
struct ExecutionAuthority
{
    double mainAccelerationMps2 = 0.0;
    double feedbackReserveMps2 = 0.0;
    double plannedVectorAccelerationMps2 = 0.0;
};

[[nodiscard]] inline ExecutionAuthority makeExecutionAuthority(
    const ShipParams& params,
    double executionFraction = 0.90,
    double feedbackReserveFraction = 0.20
) noexcept
{
    const double main =
        makeNavigationControlCapability(
            params, LocalFlightControlLaw::Newtonian).mainForwardMps2;
    const double reserveFraction =
        std::clamp(feedbackReserveFraction, 0.0, 1.0);
    const double reserve = main * reserveFraction;
    const double fraction =
        std::clamp(executionFraction, 0.0, 1.0);

    ExecutionAuthority out;
    out.mainAccelerationMps2 = main;
    out.feedbackReserveMps2 = reserve;
    out.plannedVectorAccelerationMps2 =
        std::max(0.0, (main - reserve) * fraction);
    return out;
}

} // namespace game::navigation::newtonian
