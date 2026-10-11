#pragma once

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"

namespace game::navigation::traffic
{

// Composes one continuous executable route from ordinary free-space planner
// legs separated by authored mandatory traffic-volume segments.
//
// Traffic semantics stay above RoutePlanner. Each free leg is an ordinary
// RoutePlanner request with a start state and a terminal boundary state.
// The planner never receives the internal points of another semantic stage.
class SegmentedRouteComposer final
{
public:
    [[nodiscard]] static planner::RoutePlan plan(
        const planner::RoutePlanRequest& terminalRequest,
        const CompiledTrafficRoute& trafficRoute
    );
};

} // namespace game::navigation::traffic
