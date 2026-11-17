#pragma once

#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"

namespace game::navigation::traffic
{

/*
    A docking request INTO a hub with a lane network (HubLaneCatalog), kept out of the client docking state machine
    (audit 2026-11-18). The caller (SpaceStateDocking) resolves the network and the replicated pose of its frame
    module; this module turns them into planning facts:
      - the network's RED zones join the request's obstacles;
      - the hub speed limit caps the route speed (a route whose goal lies in the hub's zone);
      - the cheapest lane chain (HubLaneNetwork::chooseRoute), searched with the free-leg demands of the SAME policy
        the SegmentedRouteComposer applies, becomes the compiled traffic route and the request's via points /
        mandatory tangent straights.
*/

// a hub module's replicated attachment (hub-local visual frame), turning at its angular velocity
struct HubModuleAttachmentPose
{
    glm::dvec3 localOffsetMeters {0.0};
    glm::dvec3 localRotationDeg {0.0};
    glm::dvec3 localAngularVelocityDegPerSecond {0.0};
};

// the module's frame in the docking planning (tactical) frame at universeTimeSeconds
[[nodiscard]] NavigationReferenceFramePose hubModulePlanningFrame(
    const HubModuleAttachmentPose& attachment,
    double universeTimeSeconds
);

// what the planner will demand of the lane network's free legs for this request (OrdinaryRoutePolicy + the
// composer's release straight); laneHandoffSpeedMps = the slowest lane speed the ship hands over at
[[nodiscard]] HubLaneNetwork::FreeLegModel freeLegModelFor(
    const planner::RoutePlanRequest& request,
    double laneHandoffSpeedMps
);

// Applies the network to the request (see above). Returns the compiled lane route (valid = use it; invalid = plan
// directly, the red zones and the speed limit still applied). log: one diagnostic line.
[[nodiscard]] CompiledTrafficRoute applyHubLanes(
    const HubLaneNetwork& network,
    const NavigationReferenceFramePose& frame,
    planner::RoutePlanRequest& request,
    std::string* log
);

} // namespace game::navigation::traffic
