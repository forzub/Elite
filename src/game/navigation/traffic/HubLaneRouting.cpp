#include "src/game/navigation/traffic/HubLaneRouting.h"

#include <algorithm>
#include <sstream>

#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/game/navigation/traffic/SegmentedRouteComposer.h"

namespace game::navigation::traffic
{

NavigationReferenceFramePose hubModulePlanningFrame(const HubModuleAttachmentPose& attachment, double universeTimeSeconds)
{
    const glm::dvec3 angles =
        attachment.localRotationDeg + attachment.localAngularVelocityDegPerSecond * universeTimeSeconds;
    const glm::dmat3 visualRotation(glm::mat3(game::navigation::hubLocalEulerDegToMatrix(angles)));
    NavigationReferenceFramePose pose;
    pose.originWorldMeters = game::navigation::hubVisualToTacticalVector(attachment.localOffsetMeters);
    pose.localToWorldBasis = glm::dmat3(
        game::navigation::hubVisualToTacticalVector(visualRotation * glm::dvec3(1.0, 0.0, 0.0)),
        game::navigation::hubVisualToTacticalVector(visualRotation * glm::dvec3(0.0, 1.0, 0.0)),
        game::navigation::hubVisualToTacticalVector(visualRotation * glm::dvec3(0.0, 0.0, 1.0)));
    return pose;
}

HubLaneNetwork::FreeLegModel freeLegModelFor(const planner::RoutePlanRequest& request, double laneHandoffSpeedMps)
{
    using Policy = game::navigation::OrdinaryRoutePolicy;
    HubLaneNetwork::FreeLegModel model;
    // the inbound leg's hard terminal approach (SegmentedRouteComposer: reference distance capped at 200 m)
    model.joinApproachMeters = Policy::requiredTerminalApproachMeters(
        request.maxSpeedMps, laneHandoffSpeedMps, request.brakingMps2,
        std::min(request.terminalReferenceDistanceMeters, 200.0), request.vehicleLengthMeters);
    // the free legs' turn radius at the route speed
    model.turnRadiusMeters = request.maxSpeedMps * request.maxSpeedMps / std::max(request.lateralMps2, 1.0e-3);
    // the final leg after a lane: the release straight + the start lead kept as turn budget + the docking axis
    const double lead =
        request.assistedCompactStartLead
            ? Policy::requiredAssistedStartLeadMeters(laneHandoffSpeedMps, request.maxSpeedMps, request.brakingMps2,
                                                      request.lateralMps2, request.maxAngularVelocityRadPerSecond,
                                                      request.vehicleLengthMeters)
            : Policy::requiredStartLeadMeters(laneHandoffSpeedMps, request.maxSpeedMps, request.brakingMps2,
                                              request.lateralMps2, request.maxAngularVelocityRadPerSecond,
                                              request.vehicleLengthMeters);
    model.exitMinimumMeters =
        SegmentedRouteComposer::ReleaseProtectedStraightMeters + lead +
        Policy::requiredDockingAxisMinimumMeters(request.terminalReferenceDistanceMeters, request.vehicleLengthMeters);
    return model;
}

CompiledTrafficRoute applyHubLanes(
    const HubLaneNetwork& network,
    const NavigationReferenceFramePose& frame,
    planner::RoutePlanRequest& request,
    std::string* log
)
{
    std::ostringstream text;
    for (auto& red : network.redZoneObstacles(frame))
        request.obstacles.push_back(std::move(red));
    // the hub speed limit for a route INTO the hub (its goal within the zone; the lanes carry their own limits)
    if (network.speedLimitMps() > 0.0 &&
        glm::length(request.goalMeters - frame.originWorldMeters) <= network.speedLimitZoneMeters() &&
        request.maxSpeedMps > network.speedLimitMps())
    {
        text << "hub_speed_limit=" << network.speedLimitMps() << " (was " << request.maxSpeedMps << ") ";
        request.maxSpeedMps = network.speedLimitMps();
    }
    double handoff = request.maxSpeedMps;
    for (const auto& lane : network.lanes())
        handoff = std::min(handoff, lane.maxSpeedMps);
    for (const auto& connector : network.connectors())
        handoff = std::min(handoff, connector.maxSpeedMps);
    const auto freeLegs = freeLegModelFor(request, handoff);
    text << "free_legs{join_approach_m=" << freeLegs.joinApproachMeters << " turn_r_m=" << freeLegs.turnRadiusMeters
         << " exit_min_m=" << freeLegs.exitMinimumMeters << "} ";
    std::string summary;
    auto route = network.chooseRoute(
        frame, request.startMeters, request.goalMeters, request.vehicleLengthMeters,
        request.hasInitialForward ? request.initialForward : glm::dvec3(0.0), request.initialForwardLeadMeters,
        -request.terminalOutward, freeLegs, &summary);
    text << summary;
    if (route.valid)
    {
        request.requiredViaPointsMeters = route.requiredViaPointsMeters;
        request.mandatoryTangentStraights.clear();
        for (const auto& straight : route.mandatoryTangentStraights)
        {
            planner::MandatoryTangentStraightConstraint constraint;
            constraint.startMeters = straight.startWorldMeters;
            constraint.endMeters = straight.endWorldMeters;
            constraint.minimumStraightMeters = straight.lengthMeters;
            constraint.inbound = straight.inbound;
            request.mandatoryTangentStraights.push_back(constraint);
        }
    }
    if (log)
        *log = text.str();
    return route;
}

} // namespace game::navigation::traffic
