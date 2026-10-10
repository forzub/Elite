#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"

namespace game::navigation::traffic
{

// Explicit semantic -> generic route-contract boundary.
//
// Traffic topology/volume semantics end here. Downstream planner/follower code
// consumes only planner::RoutePlanRequest / planner::RoutePlan value contracts
// and must not include traffic headers or inspect portal/lane/volume types.
class TrafficRoutePlanAdapter final
{
public:
    struct Result
    {
        bool valid = false;
        std::string failure;
        std::vector<planner::RouteStageSpan> stages;
        std::vector<planner::RouteFrameAnchor> frameAnchors;
    };

    [[nodiscard]] static Result buildStageContract(
        const CompiledTrafficRoute& trafficRoute,
        const planner::RoutePlan& geometricPlan,
        const glm::dvec3& terminalUpReference
    );
};

} // namespace game::navigation::traffic
