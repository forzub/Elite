#pragma once

#include "src/game/navigation/planner/assisted/AssistedRoutePlanner.h"
#include "src/game/navigation/planner/RouteEntryBinder.h"
#include "src/game/navigation/traffic/SegmentedRouteComposer.h"

namespace game::navigation::planner::assisted
{

class AssistedPlanningPipeline final
{
public:
    [[nodiscard]] static RoutePlan route(const RoutePlanRequest& input)
    {
        return AssistedRoutePlanner::plan(input);
    }

    [[nodiscard]] static RoutePlan bind(
        const RoutePlanRequest& input,
        const LateBoundRouteBackbone& backbone
    )
    {
        RoutePlanRequest request = input;
        request.roundTurns = true;
        request.useContinuousAssistedOptimizer = true;
        return RouteEntryBinder::bind(request, backbone);
    }

    [[nodiscard]] static RoutePlan segmented(
        const RoutePlanRequest& input,
        const traffic::CompiledTrafficRoute& route
    )
    {
        RoutePlanRequest request = input;
        request.roundTurns = true;
        request.useContinuousAssistedOptimizer = true;
        return traffic::SegmentedRouteComposer::plan(request, route);
    }
};

} // namespace game::navigation::planner::assisted
