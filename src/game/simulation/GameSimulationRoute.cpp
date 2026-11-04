// Route autopilot (server): a ship flies a planned route of waypoints - a hub,
// an orbit of a body, a point - on the J drive and finishes each one matched to
// the destination's motion.
//
//   hub    - arrive inside the hub's J boundary (HubJBoundaryM) with its velocity,
//            leave J automatically (a hub capture: the ship is re-based into the
//            hub frame at rest - a stable position within the hub bounds); a
//            waypoint action (refuel) then runs at the hub's dock;
//   orbit  - arrive on a circular orbit of the body at the given altitude (the
//            direction the ship comes from, prograde in the orbit plane it
//            arrives in) and leave J into the ship's own ballistic frame, which
//            falls freely under gravity: the orbit holds by itself;
//   point  - arrive at rest against the dominant body at a world point.
//
// Departure from a hub: inside its J boundary the drive is gentle (<= 1 g, <=
// 300 m/s against the hub) - no J manoeuvres near traffic; the full drive (1 g
// doubling every 2 s up to the cap) only outside it. Every massive body on the
// way is gone round (the line to the aim may not pass within R + 150 km / the
// atmosphere + 50 km of a planet / moon, nor below the destination's altitude), the speed profile is the J autopilot's: sqrt(2 a d) braking at
// 0.4 of the cap and an exponential tail d / 6 s against the destination.
// The planner (game::navigation::route) decides the waypoints; this file only
// flies them.
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/GravityFieldSystem.h"

namespace
{
constexpr double HubJBoundaryM = 20000.0;      // J on / off distance from a hub (traffic safety)
constexpr double HubArrivalM = 12000.0;        // the aim inside the boundary, on the approach side
constexpr double GentleAccelMps2 = 9.81;
constexpr double GentleSpeedMps = 300.0;
constexpr double ApproachTauS = 6.0;
constexpr double MaxDriveMps2 = 100000.0;
}

bool GameSimulation::startFreeFlightLeg(EntityId shipId, const FreeFlightLeg& leg)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& motion = ship->core().transform().motion;
    const bool alreadyInJ = motion.freeFlight;   // a route continuing past a guide point
    if (!motion.freeFlight && !toggleFreeFlight(shipId))
    {
        std::cerr << "[Route] ship " << shipId.value << ": cannot enter J for the leg" << std::endl;
        return false;
    }
    const auto it = m_freeFlights.find(shipId);
    if (it == m_freeFlights.end())
        return false;
    auto& f = it->second;
    f.autopilotTargetId.clear();          // the body autopilot gives way
    f.leg = leg;
    f.legActive = true;
    f.legSeconds = 0.0;
    f.legLastLog = -1.0e30;
    // continuing in J (past a guide point): the drive stays at full strength
    f.legDriveStartSeconds = alreadyInJ ? -1000.0 : -1.0;
    f.legHaveLastBody = false;
    f.legArrived = false;
    f.legDepartureHubId = f.sourceHubId;   // the hub left on entering J
    motion.freeFlightThrust = 0;
    motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    std::cerr << "[Route] ship " << shipId.value << ": leg -> "
              << (leg.kind == FreeFlightLeg::Kind::Hub ? "hub " + leg.hubId
                  : leg.kind == FreeFlightLeg::Kind::Orbit ? "orbit of " + leg.bodyId + " at " +
                                                                 std::to_string(static_cast<int>(leg.orbitAltitudeM / 1000.0)) + " km"
                                                           : std::string("point"))
              << " (departure " << f.legDepartureHubId << ")" << std::endl;
    return true;
}

void GameSimulation::updateFreeFlightLeg(EntityId shipId, Ship& ship, ActiveFreeFlight& f, double dt)
{
    using game::navigation::GravityBody;
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    auto& frame = motion.travelFrame;
    f.legSeconds += dt;
    const FreeFlightLeg& leg = f.leg;

    // ---- destination kinematics: position to reach, velocity to match there
    glm::dvec3 targetPos(0.0), targetVel(0.0);
    const GravityBody* body = nullptr;
    for (const auto& b : m_gravityBodies)
        if (b.id == leg.bodyId)
            body = &b;
    if (leg.kind == FreeFlightLeg::Kind::Hub)
    {
        const auto* hub = hubNavigationFrame(leg.hubId);
        if (!hub || !hub->valid)
        {
            std::cerr << "[Route] ship " << shipId.value << ": hub " << leg.hubId << " unknown - leg dropped" << std::endl;
            f.legActive = false;
            return;
        }
        // the arrival point inside the J boundary, on the side the ship comes from
        const glm::dvec3 fromHub = frame.originMeters - hub->originMeters;
        const glm::dvec3 side = glm::length(fromHub) > 1.0 ? glm::normalize(fromHub) : hub->radialAxis;
        targetPos = hub->originMeters + side * HubArrivalM;
        targetVel = hub->velocityMetersPerSecond;
    }
    else if (leg.kind == FreeFlightLeg::Kind::Orbit)
    {
        if (!body)
        {
            f.legActive = false;
            return;
        }
        const glm::dvec3 bodyVel = f.legHaveLastBody && dt > 0.0 ? (body->centerMeters - f.legLastBodyCentre) / dt : glm::dvec3(0.0);
        f.legLastBodyCentre = body->centerMeters;
        if (!f.legHaveLastBody)
        {
            f.legHaveLastBody = true;
            return;
        }
        const glm::dvec3 rel = frame.originMeters - body->centerMeters;
        const glm::dvec3 up = glm::length(rel) > 1.0 ? glm::normalize(rel) : glm::dvec3(0, 1, 0);
        const double r = body->radiusMeters + leg.orbitAltitudeM;
        // prograde: the ship's own motion across the radius (else any perpendicular)
        const glm::dvec3 w = frame.linearVelocityMps - bodyVel;
        glm::dvec3 along = w - up * glm::dot(w, up);
        if (glm::length(along) < 1.0)
            along = glm::cross(up, std::abs(up.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0));
        along = glm::normalize(along);
        targetPos = body->centerMeters + up * r;
        targetVel = bodyVel + along * std::sqrt(body->gravitationalParameterM3s2 / r);
    }
    else
    {
        targetPos = leg.pointWorld;
        targetVel = motion.freeFlightReferenceVelocityMps;
    }

    const glm::dvec3 toAim = targetPos - frame.originMeters;
    const double d = glm::length(toAim);
    glm::dvec3 dir = d > 1.0e-3 ? toAim / d : glm::dvec3(0.0);
    const glm::dvec3 w = frame.linearVelocityMps - targetVel;

    // ---- the drive: gentle inside the departure hub's J boundary, then 1 g doubling every 2 s
    const auto* depHub = hubNavigationFrame(f.legDepartureHubId);
    const double fromDep = depHub && depHub->valid ? glm::length(frame.originMeters - depHub->originMeters) : 1.0e30;
    const bool nearDeparture = fromDep < HubJBoundaryM && leg.hubId != f.legDepartureHubId;
    if (!nearDeparture && f.legDriveStartSeconds == -1.0)
        f.legDriveStartSeconds = f.legSeconds;
    const double driveSeconds = f.legDriveStartSeconds == -1.0 ? 0.0 : f.legSeconds - f.legDriveStartSeconds;
    double limit = nearDeparture ? GentleAccelMps2 : std::min(MaxDriveMps2, 9.81 * std::exp2(driveSeconds / 2.0));

    // ---- obstacles: never through a planet / moon - go round the first one in the way
    int phase = 2;
    for (const auto& b : m_gravityBodies)
    {
        if (b.radiusMeters < 50000.0 || d < 1.0)
            continue;
        // the body, its atmosphere and a margin; a destination below that (a hub in
        // low orbit) still keeps the path from dipping under the destination's own
        // altitude (a hub behind the planet was reached THROUGH it)
        double clear = std::max(b.radiusMeters + 150000.0, b.atmosphereRadiusMeters + 50000.0);
        clear = std::min(clear, glm::length(targetPos - b.centerMeters) * 0.995);
        clear = std::min(clear, glm::length(frame.originMeters - b.centerMeters) * 0.995);
        const glm::dvec3 s = frame.originMeters - b.centerMeters;
        const double t = std::clamp(-glm::dot(s, toAim) / (d * d), 0.0, 1.0);
        if (t > 0.0 && t < 1.0 && glm::length(s + toAim * t) < clear)
        {
            glm::dvec3 sideStep = s - dir * glm::dot(s, dir);
            if (glm::length(sideStep) < 1.0)
                sideStep = glm::cross(dir, std::abs(dir.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
            dir = glm::normalize(glm::normalize(sideStep) + dir * 0.3);
            phase = 1;
            break;
        }
    }

    // a pass-through guide point: brake for the rest of the route, not for the point
    const double dEff = d + (leg.passThrough ? leg.remainingAfterM : 0.0);
    const double braking = std::sqrt(2.0 * 0.4 * MaxDriveMps2 * dEff);
    const double tail = dEff / ApproachTauS;
    double speed = std::min(braking, tail);
    if (nearDeparture)
        speed = std::min(speed, GentleSpeedMps);
    if (phase == 2 && tail < braking)
        phase = 3;
    glm::dvec3 wanted = dir * speed;
    glm::dvec3 dv = wanted - w;
    if (nearDeparture && depHub)
    {
        // inside the departure hub's J boundary: a slow climb out AGAINST THAT HUB
        // (its traffic), not against the destination - the destination's speed is
        // matched only after the boundary
        const glm::dvec3 away = glm::normalize(frame.originMeters - depHub->originMeters + dir * 1.0);
        const glm::dvec3 wDep = frame.linearVelocityMps - depHub->velocityMetersPerSecond;
        dv = away * GentleSpeedMps - wDep;
    }
    const double maxDv = limit * dt;
    if (glm::length(dv) > maxDv)
        dv *= maxDv / glm::length(dv);
    frame.linearVelocityMps += dv;
    const glm::dvec3 accel = dt > 0.0 ? dv / dt : glm::dvec3(0.0);

    // ---- attitude: nose on the destination during the transfer, along the motion at the end
    {
        glm::dvec3 nose = d > 50.0 ? dir : (glm::length(w + dv) > 1.0 ? glm::normalize(w + dv) : glm::dvec3(tr.forward()));
        glm::dvec3 upW = glm::dvec3(tr.up()) - nose * glm::dot(glm::dvec3(tr.up()), nose);
        if (glm::length(upW) < 1.0e-3)
            upW = glm::cross(nose, std::abs(nose.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
        upW = glm::normalize(upW);
        const glm::dquat current = glm::quat_cast(glm::dmat3(glm::mat3(tr.orientation)));
        glm::dquat wantedQ = glm::quat_cast(glm::dmat3(glm::cross(nose, upW), upW, -nose));
        if (glm::dot(current, wantedQ) < 0.0)
            wantedQ = -wantedQ;
        const double angle = glm::angle(wantedQ * glm::inverse(current));
        const double step = 2.0 * dt;
        const glm::dquat next = angle > step ? glm::slerp(current, wantedQ, step / angle) : wantedQ;
        tr.orientation = glm::mat4(glm::mat3(glm::mat3_cast(next)));
        tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
    }

    // ---- HUD (the J autopilot panel)
    const glm::dvec3 wAfter = w + dv;
    motion.freeFlightAutopilotPhase = static_cast<std::uint8_t>(nearDeparture ? 9 : phase);
    motion.freeFlightAutopilotTargetId = leg.kind == FreeFlightLeg::Kind::Hub ? leg.hubId : leg.bodyId;
    motion.freeFlightAutopilotRangeM = d;
    motion.freeFlightAutopilotRelSpeedMps = glm::length(wAfter);
    motion.freeFlightAutopilotAccelMps2 = static_cast<float>(
        glm::length(wAfter) > 1.0e-3 ? glm::dot(accel, glm::normalize(wAfter)) : glm::length(accel));

    if (f.legSeconds - f.legLastLog >= 2.0)
    {
        f.legLastLog = f.legSeconds;
        std::cerr << "[Route] ship " << shipId.value << " t+" << f.legSeconds << " s phase " << int(motion.freeFlightAutopilotPhase)
                  << ": to aim " << d / 1000.0 << " km, speed vs destination " << glm::length(wAfter) << " m/s, accel "
                  << glm::length(accel) << " m/s2" << std::endl;
    }

    // ---- a guide point passed: the route goes on in J (no stop, no matching)
    // passed when within 1 % of the rest of the route, 20 km .. 2000 km (a blue gate is
    // ~15 000 km wide, a corridor guide is passed close to its axis)
    if (leg.passThrough && d < std::clamp(0.01 * leg.remainingAfterM, 20000.0, 2.0e6))
    {
        f.legActive = false;
        std::cerr << "[Route] ship " << shipId.value << ": passed a guide point at " << d / 1000.0 << " km, "
                  << glm::length(wAfter) / 1000.0 << " km/s" << std::endl;
        if (auto r = m_routes.find(shipId); r != m_routes.end())
            r->second.legDone = true;
        return;
    }
    // ---- finish: matched to the destination -> leave J
    // an orbit is entered precisely (3 m/s: a 20 m/s error made it ~1 % eccentric)
    const bool there = leg.kind == FreeFlightLeg::Kind::Orbit ? d < 500.0 && glm::length(wAfter) < 3.0
                                                              : d < 1500.0 && glm::length(wAfter) < 20.0;
    if (there)
    {
        f.legActive = false;
        f.legArrived = true;
        std::cerr << "[Route] ship " << shipId.value << ": ARRIVED after " << f.legSeconds << " s, " << d << " m from the aim, "
                  << glm::length(wAfter) << " m/s vs destination - leaving J" << std::endl;
        m_pendingJExit.push_back(shipId);
        if (auto r = m_routes.find(shipId); r != m_routes.end())
        {
            r->second.legDone = true;
            r->second.waitingExit = true;
        }
    }
}

glm::dvec3 GameSimulation::routePointWorld(const FreeFlightLeg& leg) const
{
    if (leg.kind == FreeFlightLeg::Kind::Hub)
    {
        const auto it = m_hubNavigationFrames.find(leg.hubId);
        return it != m_hubNavigationFrames.end() ? it->second.originMeters : leg.pointWorld;
    }
    if (leg.kind == FreeFlightLeg::Kind::Orbit)
        for (const auto& b : m_gravityBodies)
            if (b.id == leg.bodyId)
                return b.centerMeters;
    return leg.pointWorld;
}

bool GameSimulation::startRoute(EntityId shipId, std::vector<FreeFlightLeg> legs)
{
    if (legs.empty())
        return false;
    ActiveRoute route;
    route.legs = std::move(legs);
    m_routes[shipId] = route;
    std::cerr << "[Route] ship " << shipId.value << ": ROUTE of " << m_routes[shipId].legs.size() << " legs:";
    for (const auto& l : m_routes[shipId].legs)
        std::cerr << " [" << (l.kind == FreeFlightLeg::Kind::Hub ? "hub " + l.hubId : l.kind == FreeFlightLeg::Kind::Orbit ? "orbit " + l.bodyId : std::string("point"))
                  << (l.passThrough ? " pass" : " stop") << (l.action.empty() ? "" : " " + l.action) << ", rest "
                  << l.remainingAfterM / 1000.0 << " km]";
    std::cerr << std::endl;
    if (!startFreeFlightLeg(shipId, m_routes[shipId].legs.front()))
    {
        m_routes.erase(shipId);
        return false;
    }
    return true;
}

void GameSimulation::cancelRoute(EntityId shipId)
{
    if (m_routes.erase(shipId))
        std::cerr << "[Route] ship " << shipId.value << ": route cancelled" << std::endl;
    if (const auto it = m_freeFlights.find(shipId); it != m_freeFlights.end())
        it->second.legActive = false;
}

void GameSimulation::updateRoutes(double dt)
{
    for (auto it = m_routes.begin(); it != m_routes.end();)
    {
        const EntityId id = it->first;
        ActiveRoute& route = it->second;
        Ship* ship = getShip(id);
        if (!ship)
        {
            it = m_routes.erase(it);
            continue;
        }
        if (!route.legDone)
        {
            ++it;
            continue;
        }
        const FreeFlightLeg& done = route.legs[route.index];
        // a stop: wait for the J exit (hub capture / local frame), then its action
        if (route.waitingExit && ship->core().transform().motion.freeFlight)
        {
            ++it;
            continue;
        }
        route.waitingExit = false;
        if (!done.passThrough && done.action == "refuel")
        {
            // the waypoint action at the hub. PROPOSAL: the dock is not flown yet (automatic
            // docking exists only at Vysokiy's dock module) and ships carry no fuel model -
            // the service is a timed stay at the hub: dock, refuel, undock.
            if (route.serviceSeconds < 0.0)
            {
                route.serviceSeconds = 0.0;
                std::cerr << "[Route] ship " << id.value << ": at " << done.hubId << " - docking for refuel" << std::endl;
            }
            route.serviceSeconds += dt;
            if (route.serviceSeconds < 20.0)
            {
                ++it;
                continue;
            }
            std::cerr << "[Route] ship " << id.value << ": refuelled at " << done.hubId << ", undocked" << std::endl;
        }
        route.serviceSeconds = -1.0;
        route.legDone = false;
        if (++route.index >= route.legs.size())
        {
            std::cerr << "[Route] ship " << id.value << ": ROUTE COMPLETE" << std::endl;
            it = m_routes.erase(it);
            continue;
        }
        if (!startFreeFlightLeg(id, route.legs[route.index]))
        {
            std::cerr << "[Route] ship " << id.value << ": cannot start leg " << route.index << " - route stopped" << std::endl;
            it = m_routes.erase(it);
            continue;
        }
        ++it;
    }
}
