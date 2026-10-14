#pragma once

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
        return NewtonianRoutePlanner::plan(input);
    }

    [[nodiscard]] static RoutePlan bind(
        const RoutePlanRequest& input,
        const LateBoundRouteBackbone& backbone
    )
    {
        RoutePlanRequest request = input;
        request.roundTurns = false;
        request.useContinuousAssistedOptimizer = false;
        return RouteEntryBinder::bind(request, backbone);
    }

    [[nodiscard]] static RoutePlan segmented(
        const RoutePlanRequest& input,
        const traffic::CompiledTrafficRoute& route
    )
    {
        RoutePlanRequest request = input;
        request.roundTurns = false;
        request.useContinuousAssistedOptimizer = false;
        return traffic::SegmentedRouteComposer::plan(request, route);
    }
};

} // namespace game::navigation::planner::newtonian
