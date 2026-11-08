// The planned route flown by the CLIENT (NAVIGATION_V2 §1, author decision 2026-11-06). Its legs
// (route::RoutePoint, the planner's output) run here in order: the J legs on the route-leg pilot
// (globalflight/RouteLegPilot), a landing on the body autopilot; a stop ends in the stabilization
// the plant finds (a hub capture, the ground, the own orbit); a stop's action (refuel / repair) is a
// timed service the SERVER runs at the hub (it fills the J charge - an authority action); "pause"
// hands the controls to the pilot until RESUME. The server only executes the commands, judges the
// zones and keeps the charge.
#include "src/game/SpaceStateInternal.h"

#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/rules/JDrive.h"

namespace
{
namespace gf = game::navigation::globalflight;
namespace route = game::navigation::route;
}

void SpaceState::startClientRoute(const std::vector<route::RoutePoint>& legs)
{
    if (legs.empty())
        return;
    cancelClientBodyAutopilot();
    m_clientRoute = ClientRouteRun {};
    m_clientRoute.legs = legs;
    // the rest of the route after each pass-through point (straight-line estimate)
    m_clientRoute.remainingAfterM.assign(legs.size(), 0.0);
    double remaining = 0.0;
    for (std::size_t k = legs.size(); k-- > 1;)
    {
        remaining += glm::length(legs[k].worldM - legs[k - 1].worldM);
        const bool pass = legs[k - 1].kind == route::RoutePoint::Kind::Point && !legs[k - 1].stop;
        m_clientRoute.remainingAfterM[k - 1] = remaining;
        if (!pass)
            remaining = 0.0;
    }
    m_clientRoute.state = ClientRouteRun::State::Start;
    m_clientRouteLastTick = 0;
    std::cerr << "[Route] ROUTE of " << legs.size() << " legs (client):";
    for (const auto& l : legs)
        std::cerr << " [" << (l.kind == route::RoutePoint::Kind::Hub ? "hub " + l.hubId : l.kind == route::RoutePoint::Kind::Orbit ? "orbit " + l.bodyId
                              : l.kind == route::RoutePoint::Kind::Land ? "land " + l.bodyId + " " + l.landingSpec
                              : l.kind == route::RoutePoint::Kind::Ship ? "ship " + std::to_string(l.shipEntityId) : std::string("point"))
                  << (l.stop ? " stop" : " pass") << (l.action.empty() ? "" : " " + l.action) << "]";
    std::cerr << std::endl;
}

void SpaceState::cancelClientRoute()
{
    if (m_clientRoute.state == ClientRouteRun::State::Idle)
        return;
    std::cerr << "[Route] route cancelled (client)" << std::endl;
    m_clientRoute = ClientRouteRun {};
    cancelClientBodyAutopilot();
    if (m_clientRouteOwnsControl)
    {
        m_clientRouteOwnsControl = false;
        m_clientAutopilotControlActive = false;
        m_clientAutopilotControl = {};
    }
}

void SpaceState::sendRouteControl(bool pause)
{
    auto& r = m_clientRoute;
    if (pause)
    {
        if (r.state == ClientRouteRun::State::Idle || r.state == ClientRouteRun::State::Paused)
            return;
        // mid-leg: the leg starts again on RESUME; at a point: on to the next one
        r.resumeCurrent = r.state == ClientRouteRun::State::Leg || r.state == ClientRouteRun::State::Landing;
        r.pilot.stop();
        cancelClientBodyAutopilot();
        r.state = ClientRouteRun::State::Paused;
        std::cerr << "[Route] PAUSED by the pilot at leg " << r.index + 1 << " (client)" << std::endl;
        return;
    }
    if (r.state != ClientRouteRun::State::Paused)
        return;
    if (!r.resumeCurrent)
        ++r.index;
    r.resumeCurrent = false;
    if (r.index >= r.legs.size())
    {
        std::cerr << "[Route] RESUME - nothing left, ROUTE COMPLETE" << std::endl;
        r = ClientRouteRun {};
        return;
    }
    std::cerr << "[Route] RESUMED - leg " << r.index + 1 << " of " << r.legs.size() << std::endl;
    r.state = ClientRouteRun::State::Start;
}

int SpaceState::clientRouteState() const
{
    using S = ClientRouteRun::State;
    switch (m_clientRoute.state)
    {
    case S::Idle: return 0;
    case S::Paused: return 2;
    case S::Service: return 3;
    case S::Landing: return 4;
    default: return 1;
    }
}

void SpaceState::updateClientRoute()
{
    using S = ClientRouteRun::State;
    auto& r = m_clientRoute;
    if (!m_client || r.state == S::Idle)
        return;
    const auto& meta = m_client->lastSimulationMetadata();
    const bool owns = r.state == S::Leg || r.state == S::WaitingExit || r.state == S::Start;
    if (meta.serverTick == m_clientRouteLastTick)
    {
        // between samples: keep the last command in the stream
        if (owns && m_clientRouteOwnsControl)
        {
            m_clientAutopilotControlActive = true;
            m_client->submitInput(m_clientAutopilotControl);
        }
        return;
    }
    const double dt = m_clientRouteLastTick != 0 ? meta.serverTimeSeconds - m_clientRouteLastServerTime : 0.0;
    m_clientRouteLastTick = meta.serverTick;
    m_clientRouteLastServerTime = meta.serverTimeSeconds;
    gf::BodyAutopilotEnvironment env;
    gf::BodyAutopilotShip sample;
    if (!clientBodyAutopilotWorld(env, sample))
        return;
    const auto& motion = m_client->world().ships().at(m_playerId.value).transform.motion;
    const auto release = [&] {
        if (m_clientRouteOwnsControl)
        {
            m_clientRouteOwnsControl = false;
            m_clientAutopilotControlActive = false;
            m_clientAutopilotControl = {};
        }
    };
    if (motion.destroyedCause != 0)
    {
        std::cerr << "[Route] the ship is DESTROYED - route stopped" << std::endl;
        release();
        r = ClientRouteRun {};
        return;
    }
    const auto submit = [&](const ShipControlState& control) {
        m_clientAutopilotControl = control;
        m_clientAutopilotControlActive = true;
        m_clientRouteOwnsControl = true;
        m_client->submitInput(m_clientAutopilotControl);
    };
    const auto finishPoint = [&] {
        // after a stop / a landing: its action, then the next leg
        const auto& done = r.legs[r.index];
        if (done.stop && (done.action.find("refuel") != std::string::npos || done.action.find("repair") != std::string::npos) &&
            r.serviceStart < 0.0)
        {
            r.serviceStart = meta.serverTimeSeconds;
            r.state = S::Service;
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::RouteService;   // the server fills the J charge
            command.target = done.hubId;
            command.detail = done.action;
            msg.payload = command;
            m_client->sendMessage(msg);
            std::cerr << "[Route] at " << done.hubId << " - docking for " << done.action << " (the server's service stay)" << std::endl;
            release();
            return;
        }
        r.serviceStart = -1.0;
        if (++r.index >= r.legs.size())
        {
            std::cerr << "[Route] ROUTE COMPLETE (client)" << std::endl;
            r = ClientRouteRun {};
            release();
            return;
        }
        if (done.action.find("pause") != std::string::npos)
        {
            --r.index;   // resume steps to the next leg
            r.resumeCurrent = false;
            r.state = S::Paused;
            std::cerr << "[Route] PAUSED at point " << r.index + 1 << " - manual flight until RESUME" << std::endl;
            release();
            return;
        }
        r.state = S::Start;
    };

    switch (r.state)
    {
    case S::Start:
    {
        const auto& leg = r.legs[r.index];
        if (leg.kind == route::RoutePoint::Kind::Land)
        {
            std::cerr << "[Route] leg -> LANDING on " << leg.bodyId << (leg.landingSpec.empty() ? std::string(" (wild site)") : " at " + leg.landingSpec)
                      << std::endl;
            release();
            requestFreeFlightAutopilot(leg.landingSpec.rfind("site:", 0) == 0 ? std::string() : leg.bodyId, leg.landingSpec);
            // past a guide point at full J speed: the landing leg's drive keeps the ramp (no restart from 1 g)
            m_bodyAutopilotCarryRampS = motion.freeFlight && r.index > 0 && r.legs[r.index - 1].kind == route::RoutePoint::Kind::Point &&
                                                !r.legs[r.index - 1].stop
                                            ? r.lastDriveSeconds
                                            : 0.0;
            r.state = S::Landing;
            r.landingSeen = false;
            return;
        }
        r.pilot.start(leg, r.remainingAfterM[r.index], motion.freeFlight, motion.freeFlightSurfaceState == 1,
                      motion.freeFlight ? r.departureHubId : motion.hubId, sample.shipId);
        r.departureHubId = motion.freeFlight ? r.departureHubId : motion.hubId;
        // a guide point flies against the hub the route is going to (the next hub point)
        for (std::size_t k = r.index; k < r.legs.size(); ++k)
            if (r.legs[k].kind == route::RoutePoint::Kind::Hub)
            {
                r.pilot.setRouteReferenceHub(r.legs[k].hubId);
                break;
            }
            else if (r.legs[k].stop)
                break;
        r.state = S::Leg;
        [[fallthrough]];
    }
    case S::Leg:
    {
        const auto out = r.pilot.step(env, clientRouteResolver(), sample, dt);
        r.lastDriveSeconds = r.pilot.driveSeconds();
        if (out.displayPhase != 0 || !out.displayTargetId.empty())
            m_bodyAutopilotDisplay = out;
        submit(out.control);
        if (r.pilot.passed())
        {
            ++r.index;
            r.state = r.index < r.legs.size() ? S::Start : S::Idle;
        }
        else if (r.pilot.arrived())
            r.state = S::WaitingExit;
        else if (!r.pilot.active())
        {
            std::cerr << "[Route] leg " << r.index + 1 << " dropped - route stopped" << std::endl;
            r = ClientRouteRun {};
            m_bodyAutopilotDisplay = {};
            release();
        }
        return;
    }
    case S::WaitingExit:
    {
        if (motion.freeFlight)
        {
            // J off: the stabilization the plant finds (a hub capture / the ground / the own orbit)
            ShipControlState c {};
            c.globalFlightCommandValid = true;
            c.globalFrameRequest = static_cast<std::uint8_t>(gf::FrameRequest::Stabilization);
            submit(c);
            return;
        }
        m_bodyAutopilotDisplay = {};
        finishPoint();
        return;
    }
    case S::Landing:
    {
        if (motion.freeFlightSurfaceState == 1 && !m_bodyAutopilot.active())
        {
            std::cerr << "[Route] landed (leg " << r.index + 1 << ")" << std::endl;
            finishPoint();
        }
        else if (motion.freeFlightSurfaceState == 2)
        {
            std::cerr << "[Route] CRASHED on the landing leg - route stopped" << std::endl;
            r = ClientRouteRun {};
        }
        else if (m_bodyAutopilot.active() || m_bodyAutopilotEngagePending)
            r.landingSeen = true;
        else if (r.landingSeen)
        {
            std::cerr << "[Route] the landing autopilot gave up - route paused" << std::endl;
            r.state = S::Paused;
            r.resumeCurrent = true;
        }
        return;
    }
    case S::Service:
    {
        const auto& done = r.legs[r.index];
        const bool refuel = done.action.find("refuel") != std::string::npos;
        const double stay = meta.serverTimeSeconds - r.serviceStart;
        const bool full = motion.jChargeCapacityT > 0.0 && motion.jChargeT >= motion.jChargeCapacityT - 1.0e-9;
        if ((refuel && full && stay > 20.0) || stay > 25.0)
        {
            std::cerr << "[Route] service at " << done.hubId << " done (J charge " << motion.jChargeT << " t)" << std::endl;
            r.serviceStart = 1.0e300;   // done: finishPoint goes on
            finishPoint();
            r.serviceStart = -1.0;
        }
        return;
    }
    case S::Paused:
        release();
        return;
    case S::Idle:
        return;
    }
}

game::navigation::globalflight::RouteLegResolver SpaceState::clientRouteResolver() const
{
    gf::RouteLegResolver resolve;
    const auto* client = m_client;
    resolve.hub = [client](const std::string& id, glm::dvec3& p, glm::dvec3& v) {
        const auto it = client->world().hubs().find(id);
        if (it == client->world().hubs().end())
            return false;
        p = world::coordinates::fullMeters(it->second.worldPosition);
        v = it->second.worldVelocityMps;
        return true;
    };
    resolve.ship = [client](std::uint64_t id, glm::dvec3& p, glm::dvec3& v) {
        const auto it = client->world().ships().find(id);
        if (it == client->world().ships().end())
            return false;
        const auto& m = it->second.transform.motion;
        p = m.travelFrame.valid ? m.travelFrame.localToWorldPosition(m.localPositionMeters)
                                : world::coordinates::fullMeters(it->second.transform.worldPosition);
        v = m.travelFrame.valid ? m.travelFrame.localToWorldVelocity(m.localPositionMeters, m.localVelocityMps) : m.worldVelocityMps;
        return true;
    };
    resolve.hubAround = [client](const glm::dvec3& point, std::string& hubId, glm::dvec3& p, glm::dvec3& v) {
        namespace jd = game::navigation::jdrive;
        for (const auto& [id, hub] : client->world().hubs())
        {
            const glm::dvec3 h = world::coordinates::fullMeters(hub.worldPosition);
            if (glm::length(h - point) < jd::KeepOutObjectM)
            {
                hubId = id;
                p = h;
                v = hub.worldVelocityMps;
                return true;
            }
        }
        return false;
    };
    return resolve;
}
