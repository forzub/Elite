#pragma once

// Concrete per-law route autopilots used by production SpaceState and by the
// navigation tests. The route layer itself stays pilot-agnostic; only this
// header binds it to the Assisted and Newtonian controllers.

#include "src/game/navigation/autopilot/AssistedAutopilot.h"
#include "src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h"
#include "src/game/navigation/autopilot/NewtonianOptimalController.h"

namespace game::navigation::autopilot
{

using AssistedClientRouteAutopilot = DedicatedClientRouteAutopilot<
    LocalFlightControlLaw::Assisted,
    AssistedAutopilot
>;

using NewtonianClientRouteAutopilot = DedicatedClientRouteAutopilot<
    LocalFlightControlLaw::Newtonian,
    NewtonianOptimalController
>;

} // namespace game::navigation::autopilot
