#pragma once

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation::planner::assisted
{

// Hard Assisted planner boundary. Callers cannot select another flight law
// through this API. Input/output are value objects only.
class AssistedRoutePlanner final
{
public:
    [[nodiscard]] static RoutePlan plan(const RoutePlanRequest& input)
    {
        RoutePlanRequest request = input;
        request.roundTurns = true;
        request.useContinuousAssistedOptimizer = true;
        return RoutePlanner::plan(request);
    }
};

} // namespace game::navigation::planner::assisted
