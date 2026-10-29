#pragma once

#include <algorithm>
#include <string>

#include "src/game/navigation/AssistedConcurrentManeuverBudget.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/navigation/autopilot/NewtonianTurnModel.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace game::navigation
{

struct RouteRequestCapabilityResult
{
    bool ok = false;
    planner::RoutePlanFailureCode failureCode =
        planner::RoutePlanFailureCode::PropulsionInsufficient;
    std::string reason;
    // Lateral execution capability of route curves (diagnostics).
    double executionCurveLateralMps2 = 0.0;
};

// The route request's capability numbers, from the factual vehicle profile
// (NavigationControlCapability through makeNavigationVehicleProfile) and the
// explicit planning policy below. ONE implementation for the game and the
// offline tools.
//
// Policy: execution reserve on the client exactly where the virtual pilot
// lives (the planner never assumes all installed authority is available),
// 90 % planning share, Assisted concurrent-turn share, Newtonian preferred
// lateral share. Speed is not an execution authority reserve: the real
// physical maximum is published, the cruise policy applies its own factor.
//
// Zero is zero: a missing authority is NOT replaced by a small invented
// value. Without longitudinal, braking or lateral authority, or without a
// way to turn the hull, the request is refused (PropulsionInsufficient) and
// the caller must not plan; it waits or reports PhysicallyImpossible.
[[nodiscard]] inline RouteRequestCapabilityResult applyRouteRequestCapability(
    planner::RoutePlanRequest& request,
    const world::navigation::NavigationVehicleProfile& ship,
    LocalFlightControlLaw law,
    const NavigationControlCapability* facts = nullptr
)
{
    // Newtonian: the pilot's own turn model, so the route leaves the room
    // the pilot really needs for a flip.
    request.newtonianFlipSeconds =
        law == LocalFlightControlLaw::Newtonian && facts
            ? autopilot::newtonianFlipLeadSeconds(*facts) : 0.0;
    // Newtonian: obstacles are cleared by the pilot's ordinary maximum
    // cross-track (healthy flights: 18-22 m), not only by the hull radius.
    request.obstacleExecutionClearanceMeters =
        law == LocalFlightControlLaw::Newtonian ? 20.0 : 0.0;
    const bool assisted = law == LocalFlightControlLaw::Assisted;
    const double linearReserve =
        DockingAutomaticRecoveryPolicy::linearFeedbackReserveMps2(
            ship.maxForwardAccelerationMps2,
            ship.maxBrakingAccelerationMps2,
            ship.maxLateralAccelerationMps2
        );
    request.maxSpeedMps = ship.maxSpeedMps;
    request.acceleratingMps2 = std::max(
        0.0, (ship.maxForwardAccelerationMps2 - linearReserve) * 0.90);
    request.brakingMps2 = std::max(
        0.0,
        (ship.maxBrakingAccelerationMps2 - linearReserve) * 0.90 *
            (assisted ? assisted_budget::ConcurrentTurnAxisFraction : 1.0));

    RouteRequestCapabilityResult out;
    out.executionCurveLateralMps2 = std::max(
        0.0, (ship.maxLateralAccelerationMps2 - linearReserve) * 0.90);
    // Newtonian curves (route fillets and the accepted trajectory) use ONE
    // capability: the preferred routine lateral acceleration. The planner
    // only asks whether the ship can provide it; the pilot chooses the
    // actuator.
    request.lateralMps2 = assisted
        ? assisted_budget::reservedCurveLateralAccelerationMps2(
              out.executionCurveLateralMps2)
        : newtonianPreferredLateralAccelerationMps2(request.acceleratingMps2);
    request.maxAngularVelocityRadPerSecond =
        ship.maxAngularVelocityRadPerSecond;
    request.maxAngularAccelerationRadPerSecond2 =
        ship.maxAngularAccelerationRadPerSecond2 * 0.90;

    constexpr double Minimum = 1.0e-6;
    const auto missing = [&](const char* what)
    {
        if (!out.reason.empty())
            out.reason += ", ";
        out.reason += what;
    };
    if (!(request.maxSpeedMps > Minimum)) missing("speed limit");
    if (!(request.acceleratingMps2 > Minimum)) missing("acceleration");
    if (!(request.brakingMps2 > Minimum)) missing("braking");
    if (!(request.lateralMps2 > Minimum)) missing("lateral");
    if (!(request.maxAngularVelocityRadPerSecond > Minimum) ||
        !(request.maxAngularAccelerationRadPerSecond2 > Minimum))
        missing("attitude control");
    out.ok = out.reason.empty();
    if (!out.ok)
        out.reason = "propulsion insufficient: no " + out.reason;
    else
        out.failureCode = planner::RoutePlanFailureCode::None;
    return out;
}

} // namespace game::navigation
