#include "src/game/navigation/traffic/HubLaneRouting.h"

#include <algorithm>
#include <cstdlib>
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
    const double geometrySpeed = request.geometrySpeed();
    model.joinApproachMeters = Policy::requiredTerminalApproachMeters(
        geometrySpeed, laneHandoffSpeedMps, request.brakingMps2,
        std::min(request.terminalReferenceDistanceMeters, 200.0), request.vehicleLengthMeters);
    // the free legs' turn radius at the route speed
    model.turnRadiusMeters = geometrySpeed * geometrySpeed / std::max(request.lateralMps2, 1.0e-3);
    // the final leg after a lane: the release straight + the start lead kept as turn budget + the docking axis
    const double lead =
        request.assistedCompactStartLead
            ? Policy::requiredAssistedStartLeadMeters(laneHandoffSpeedMps, geometrySpeed, request.brakingMps2,
                                                      request.lateralMps2, request.maxAngularVelocityRadPerSecond,
                                                      request.vehicleLengthMeters, false)
            : Policy::requiredStartLeadMeters(laneHandoffSpeedMps, geometrySpeed, request.brakingMps2,
                                              request.lateralMps2, request.maxAngularVelocityRadPerSecond,
                                              request.vehicleLengthMeters, false);
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
    // the hub speed limit for a route INTO the hub (its goal within the zone; the lanes carry their own limits).
    // The author (2026-11-23) wants no cap in free space (30 m/s in the built-up part only) - but the free legs' turn
    // geometry is still sized for the route's max speed: without the cap (500 m/s) the lane joins grew to an 8.8 km
    // approach with a 14 km turn radius, no lane entry fitted and the optimizer found no path (planmove v2). So the
    // cap stays ON until the turn geometry is decoupled from the max speed (fast on straights, braking for turns).
    // ELITE_HUB_ZONE_SPEED_CAP=0 turns it off (for that work).
    static const bool zoneCap = [] {
        const char* v = std::getenv("ELITE_HUB_ZONE_SPEED_CAP");
        return !(v && v[0] == '0');
    }();
    // ELITE_HUB_FREE_SPEED=1 (experiment): the zone limit sizes only the GEOMETRY (geometrySpeedMps), the speed itself
    // stays the ship's - the profile brakes for the turns and the lanes keep their own limits
    static const bool freeSpeed = [] {
        const char* v = std::getenv("ELITE_HUB_FREE_SPEED");
        return v && v[0] == '1';
    }();
    if (zoneCap && network.speedLimitMps() > 0.0 &&
        glm::length(request.goalMeters - frame.originWorldMeters) <= network.speedLimitZoneMeters() &&
        request.maxSpeedMps > network.speedLimitMps())
    {
        if (freeSpeed)
        {
            text << "hub_geometry_speed=" << network.speedLimitMps() << " (speed " << request.maxSpeedMps << ") ";
            request.geometrySpeedMps = network.speedLimitMps();
        }
        else
        {
            text << "hub_speed_limit=" << network.speedLimitMps() << " (was " << request.maxSpeedMps << ") ";
            request.maxSpeedMps = network.speedLimitMps();
        }
    }
    // a craft flies the hub no faster than it can still TURN and STOP within ~1.5 km (a weak one - the cockpit-capsule's
    // 2 m/s2 jets - got a 11.7 km turn radius and a 7 km join approach at the hub's 150 m/s: no lane entry fitted, no
    // path; live 2026-11-25). A full ship (Cobrett ~17 m/s2 lateral) is not touched: its figure is above the hub limit.
    {
        constexpr double ManoeuvreRoomMeters = 1500.0;
        const double byTurn = std::sqrt(std::max(0.0, request.lateralMps2) * ManoeuvreRoomMeters);
        const double byStop = std::sqrt(2.0 * std::max(0.0, request.brakingMps2) * ManoeuvreRoomMeters);
        const double craftCap = std::max(20.0, std::min(byTurn, byStop));
        if (craftCap < request.maxSpeedMps)
        {
            text << "craft_speed_cap=" << craftCap << " (lateral " << request.lateralMps2 << ", braking "
                 << request.brakingMps2 << ") ";
            request.maxSpeedMps = craftCap;
            if (request.geometrySpeedMps > craftCap)
                request.geometrySpeedMps = craftCap;
        }
    }
    double handoff = request.geometrySpeed();
    for (const auto& lane : network.lanes())
        handoff = std::min(handoff, lane.maxSpeedMps);
    for (const auto& connector : network.connectors())
        handoff = std::min(handoff, connector.maxSpeedMps);
    auto freeLegs = freeLegModelFor(request, handoff);
    freeLegs.freeSpeedMps = request.maxSpeedMps;   // the chain is costed in time at the free legs' real cruise
    freeLegs.goalByService = request.endAtLaneExit;
    freeLegs.serviceDirect = request.serviceDirectAllowed && !request.forbidDirectFreeLeg;
    freeLegs.forbidDirect = request.forbidDirectFreeLeg;
    if (request.forbidDirectFreeLeg)
        text << "recovery=lanes_only ";
    text << "free_legs{join_approach_m=" << freeLegs.joinApproachMeters << " turn_r_m=" << freeLegs.turnRadiusMeters
         << " exit_min_m=" << freeLegs.exitMinimumMeters << "} ";
    std::string summary;
    auto route = network.chooseRoute(
        frame, request.startMeters, request.goalMeters, request.vehicleLengthMeters,
        request.hasInitialForward ? request.initialForward : glm::dvec3(0.0), request.initialForwardLeadMeters,
        -request.terminalOutward, freeLegs, &summary, &request.obstacles, request.agentRadiusMeters);
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
