#pragma once

#include <algorithm>

#include "src/game/navigation/planner/newtonian/NewtonianRoutePlanner.h"
#include "src/game/navigation/planner/RouteEntryBinder.h"
#include "src/game/navigation/traffic/SegmentedRouteComposer.h"

namespace game::navigation::planner::newtonian
{

class NewtonianPlanningPipeline final
{
public:
    [[nodiscard]] static RoutePlan route(const RoutePlanRequest& input)
    {
        return NewtonianRoutePlanner::plan(withFlipRoom(input));
    }

    // A moving start may have to reverse its motion: the first straight
    // leaves room for the pilot's planned 180 degree flip at the start
    // speed and the brake to rest after it (the planner otherwise only
    // reserves braking distance and a turn radius).
    [[nodiscard]] static RoutePlanRequest withFlipRoom(RoutePlanRequest request)
    {
        constexpr double MovingStartMps = 2.0;
        const double v = request.initialSpeedMps;
        if (!(request.newtonianFlipSeconds > 0.0) || !(v > MovingStartMps) ||
            !(request.brakingMps2 > 1.0e-6) || !request.hasInitialForward)
            return request;
        const double room =
            v * request.newtonianFlipSeconds +
            v * v / (2.0 * request.brakingMps2);
        request.initialProtectedStraightMeters =
            std::max(request.initialProtectedStraightMeters, room);
        request.initialForwardLeadMeters =
            std::max(request.initialForwardLeadMeters, room);
        return request;
    }

    [[nodiscard]] static RoutePlan bind(
        const RoutePlanRequest& input,
        const LateBoundRouteBackbone& backbone
    )
    {
        RoutePlanRequest request = input;
        // Corners are rounded where a fillet fits the ship's preferred
        // lateral capability; the arc is the COM path, the pilot decides
        // which actuator produces the lateral acceleration.
        request.roundTurns = true;
        request.mergeNearCollinearVertices = true;
        request.useContinuousAssistedOptimizer = false;
        return RouteEntryBinder::bind(request, backbone);
    }

    [[nodiscard]] static RoutePlan segmented(
        const RoutePlanRequest& input,
        const traffic::CompiledTrafficRoute& route
    )
    {
        RoutePlanRequest request = input;
        // Corners are rounded where a fillet fits the ship's preferred
        // lateral capability; the arc is the COM path, the pilot decides
        // which actuator produces the lateral acceleration.
        request.roundTurns = true;
        request.mergeNearCollinearVertices = true;
        request.useContinuousAssistedOptimizer = false;
        return traffic::SegmentedRouteComposer::plan(
            withFlipRoom(request), route);
    }
};

} // namespace game::navigation::planner::newtonian
