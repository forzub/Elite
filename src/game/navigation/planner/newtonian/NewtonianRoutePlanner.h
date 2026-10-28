#pragma once

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation::planner::newtonian
{

// Hard Newtonian planner boundary. No Assisted optimizer flag or mutable
// Assisted state can enter this API.
class NewtonianRoutePlanner final
{
public:
    [[nodiscard]] static RoutePlan plan(const RoutePlanRequest& input)
    {
        RoutePlanRequest request = input;
        // Corners are rounded where a fillet fits the ship's preferred
        // lateral capability; the arc is the COM path, the pilot decides
        // which actuator produces the lateral acceleration.
        request.roundTurns = true;
        request.mergeNearCollinearVertices = true;
        request.useContinuousAssistedOptimizer = false;
        return RoutePlanner::plan(request);
    }
};

} // namespace game::navigation::planner::newtonian
