// Route launch from the System Map (finish -> "official" / "unofficial") and the
// zone-violation watch. The planner is game::navigation::route (zones from
// assets/data/navigation/route_zones.json); the server's route autopilot flies
// the legs (GameSimulationRoute.cpp).
#include "src/game/SpaceStateInternal.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/game/navigation/route/RoutePlanner.h"

namespace
{
namespace route = game::navigation::route;

// the body whose Laplace sphere of influence holds a point (catalog parents; the
// star when none) - the same rule as the server's J reference body
std::string bodyAtPoint(const world::celestial::CelestialSystemSnapshot& snapshot,
                        const world::celestial::CelestialSystemDefinition* system, const glm::dvec3& p)
{
    if (!system)
        return {};
    const auto mu = [&](const std::string& id) {
        for (const auto& b : system->bodies)
            if (b.id == id)
                return b.gravitationalParameterM3s2;
        return 0.0;
    };
    const world::celestial::CelestialBodyState* root = nullptr;
    for (const auto& b : snapshot.bodies)
        if (mu(b.id) > 0.0 && (!root || mu(b.id) > mu(root->id)))
            root = &b;
    std::string best = root ? root->id : std::string();
    double bestSphere = 1.0e300;
    for (const auto& b : snapshot.bodies)
    {
        if (root && b.id == root->id)
            continue;
        const double m = mu(b.id);
        if (m <= 0.0)
            continue;
        const world::celestial::CelestialBodyState* parent = nullptr;
        for (const auto& q : snapshot.bodies)
            if (q.id == b.parentId)
                parent = &q;
        if (!parent || mu(parent->id) <= m)
            parent = root;
        if (!parent)
            continue;
        const double sphere = glm::length(parent->worldMeters - b.worldMeters) * std::pow(m / mu(parent->id), 0.4);
        if (sphere < bestSphere && glm::length(p - b.worldMeters) < sphere)
        {
            bestSphere = sphere;
            best = b.id;
        }
    }
    return best;
}

const route::ZoneCatalog& zoneCatalog()
{
    static const route::ZoneCatalog catalog = route::loadZoneCatalog();
    return catalog;
}
} // namespace

route::AnchorResolver SpaceState::routeAnchorResolver() const
{
    return [this](const route::ZoneAnchor& a, glm::dvec3& out) -> bool {
        if (!m_client)
            return false;
        const auto* snapshot = m_client->celestialSnapshot();
        const auto bodyPos = [&](const std::string& id, glm::dvec3& p) {
            if (!snapshot)
                return false;
            for (const auto& b : snapshot->bodies)
                if (b.id == id)
                {
                    p = b.worldMeters;
                    return true;
                }
            return false;
        };
        if (a.kind == "hub")
        {
            const auto it = m_client->world().hubs().find(a.id);
            if (it == m_client->world().hubs().end())
                return false;
            out = world::coordinates::fullMeters(it->second.worldPosition);
            return true;
        }
        if (a.kind == "body")
            return bodyPos(a.id, out);
        if (a.kind == "lagrange")
        {
            glm::dvec3 p, s;
            if (!bodyPos(a.primary, p) || !bodyPos(a.secondary, s))
                return false;
            out = p + (s - p) * a.fraction;
            return true;
        }
        return false;
    };
}

void SpaceState::launchPlannedRoute(bool official)
{
    if (!m_client)
        return;
    const auto shipIt = m_client->world().ships().find(m_playerId.value);
    const auto* snapshot = m_client->celestialSnapshot();
    if (shipIt == m_client->world().ships().end() || !snapshot)
        return;
    const auto* system = m_client->starAtlas() ? m_client->starAtlas()->findSystem(snapshot->systemId) : nullptr;
    const auto& plan = m_navigationWorkspace.routePlan();

    route::PlanRequest req;
    req.official = official;
    req.startM = world::coordinates::fullMeters(shipIt->second.transform.worldPosition);
    req.bodyAt = [&](const glm::dvec3& p) { return bodyAtPoint(*snapshot, system, p); };
    req.startBodyId = req.bodyAt(req.startM);
    for (const auto& [hubId, hub] : m_client->world().hubs())
        req.hubsM.emplace_back(hubId, world::coordinates::fullMeters(hub.worldPosition));

    // intermediates in their order, then the finish
    std::vector<const game::navigation::NavigationWaypoint*> order;
    for (const auto& w : plan.waypoints())
        if (w.role == game::navigation::NavigationWaypointRole::Intermediate)
            order.push_back(&w);
    std::sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->sequence < b->sequence; });
    for (const auto& w : plan.waypoints())
        if (w.role == game::navigation::NavigationWaypointRole::Finish)
            order.push_back(&w);
    if (order.empty() || order.back()->role != game::navigation::NavigationWaypointRole::Finish)
    {
        std::cerr << "[RoutePlan] no finish on the map" << std::endl;
        return;
    }
    {
        // a landing finish (a wild site / a settlement pad) is flown by the body autopilot, which
        // lands; the route legs do not land yet - intermediates are ignored for now (R2)
        using P = game::navigation::NavigationWaypointProgram;
        const auto& fin = *order.back();
        const P program = game::navigation::effectiveProgram(fin);
        if (program == P::Land || program == P::LandAtSettlement)
        {
            if (order.size() > 1)
                std::cerr << "[RoutePlan] a landing finish: " << order.size() - 1
                          << " intermediate point(s) not flown yet (R2) - straight to the landing" << std::endl;
            if (program == P::LandAtSettlement)
                requestFreeFlightAutopilot({}, fin.sourceObjectId.rfind("site:", 0) == 0 ? fin.sourceObjectId
                                                                                         : "site:" + fin.target.stableObjectId);
            else
                requestFreeFlightAutopilot(fin.target.stableObjectId);
            std::cerr << "[RoutePlan] finish program " << game::navigation::waypointProgramKey(program) << " -> the landing autopilot"
                      << std::endl;
            return;
        }
    }
    for (const auto* w : order)
    {
        const bool finish = w->role == game::navigation::NavigationWaypointRole::Finish;
        route::RoutePoint p;
        p.label = w->displayName;
        p.worldM = world::coordinates::fullMeters(w->worldPosition);
        using Anchor = game::navigation::NavigationRouteAnchorKind;
        using P = game::navigation::NavigationWaypointProgram;
        const P program = game::navigation::effectiveProgram(*w);
        if (w->target.kind == Anchor::Hub && m_client->world().hubs().count(w->target.stableObjectId))
        {
            p.kind = route::RoutePoint::Kind::Hub;
            p.hubId = w->target.stableObjectId;
            p.worldM = world::coordinates::fullMeters(m_client->world().hubs().at(p.hubId).worldPosition);
            // the services of a dock stop: "refuel", "repair", "refuel+repair"
            std::string services;
            if (w->actions.refuel || w->stopAction == "refuel")
                services = "refuel";
            if (w->actions.repair)
                services += services.empty() ? "repair" : "+repair";
            p.action = services;
            p.stop = program == P::Dock && (finish || !services.empty() || w->actions.pauseHere);
            if (!p.stop)
                p.kind = route::RoutePoint::Kind::Point;   // a hub flown past
        }
        else if (w->target.kind == Anchor::CelestialBody && (finish || program == P::Orbit || program == P::OrbitSurvey) &&
                 program != P::PassThrough)
        {
            p.kind = route::RoutePoint::Kind::Orbit;
            p.bodyId = w->target.stableObjectId;
            p.stop = true;
            for (const auto& b : snapshot->bodies)
                if (b.id == p.bodyId)
                {
                    p.worldM = b.worldMeters;
                    // a low, safe orbit: 6 % of the radius, at least 100 km (over an atmosphere); the
                    // point's own altitude when set
                    p.orbitAltitudeM = w->actions.orbitAltitudeM > 0.0 ? w->actions.orbitAltitudeM
                                                                       : std::max(100000.0, b.radiusKm * 1000.0 * 0.06);
                }
        }
        else
        {
            p.kind = route::RoutePoint::Kind::Point;
            p.stop = finish;
        }
        req.points.push_back(p);
    }

    const auto zones = route::resolveZones(zoneCatalog(), snapshot->systemId, routeAnchorResolver());
    const route::PlannedRoute planned = route::planRoute(req, zones, zoneCatalog().hubRadarM);
    std::cerr << "[RoutePlan] " << (official ? "OFFICIAL" : "UNOFFICIAL") << " route: " << planned.legs.size() << " legs, "
              << planned.lengthM / 1000.0 << " km";
    for (const auto& n : planned.notes)
        std::cerr << " | " << n;
    std::cerr << std::endl;

    // the route on the map (the transfer tunnel line, relative to the start body)
    {
        glm::dvec3 origin(0.0);
        for (const auto& b : snapshot->bodies)
            if (b.id == req.startBodyId)
                origin = b.worldMeters;
        std::vector<glm::dvec3> line {req.startM - origin};
        for (const auto& l : planned.legs)
            line.push_back(l.worldM - origin);
        m_systemMapRenderer.setTransferTunnel(req.startBodyId, std::move(line));
    }

    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::RouteLeg;
    command.index = 1;
    command.detail = route::encodeRoute(planned);
    msg.payload = command;
    m_client->sendMessage(msg);
    m_routeOfficial = official;
    m_routeGatePassed.clear();
}

void SpaceState::updateZoneViolations(double dt)
{
    m_zoneCheckSeconds -= dt;
    m_violationShowSeconds = std::max(0.0, m_violationShowSeconds - dt);
    if (!m_client || m_zoneCheckSeconds > 0.0)
        return;
    m_zoneCheckSeconds = 0.25;
    const auto shipIt = m_client->world().ships().find(m_playerId.value);
    const auto* snapshot = m_client->celestialSnapshot();
    if (shipIt == m_client->world().ships().end() || !snapshot)
        return;
    const glm::dvec3 p = world::coordinates::fullMeters(shipIt->second.transform.worldPosition);
    const auto zones = route::resolveZones(zoneCatalog(), snapshot->systemId, routeAnchorResolver());
    const auto fine = [&](const route::RouteZone& z, const std::string& what) {
        m_violationText = "АДМИНИСТРАТИВНОЕ НАРУШЕНИЕ: " + what + " «" + z.name + "»  ·  штраф " +
                          std::to_string(static_cast<long long>(z.fineCr)) + " кр  ·  " + z.authority;
        m_violationShowSeconds = 8.0;
        m_finesTotalCr += z.fineCr;
        std::cerr << "[Zones] VIOLATION: " << what << " " << z.id << ", fine " << z.fineCr << " cr (total " << m_finesTotalCr
                  << ")" << std::endl;
    };
    const auto* system = m_client->starAtlas() ? m_client->starAtlas()->findSystem(snapshot->systemId) : nullptr;
    const std::string body = bodyAtPoint(*snapshot, system, p);
    for (const auto& g : zones)
    {
        const bool inside = g.contains(p);
        bool& was = m_zoneInside[g.zone->id];
        if (inside && !was && g.zone->colour == route::ZoneColour::Red)
            fine(*g.zone, "вход в красную зону");
        if (inside && g.zone->colour == route::ZoneColour::Blue)
            m_routeGatePassed.insert(g.zone->id);
        was = inside;
        // a blue gate skipped: the ship went from one of its bodies to the other without it
        if (g.zone->colour == route::ZoneColour::Blue && !m_lastZoneBody.empty() && body != m_lastZoneBody)
        {
            const auto& gb = g.zone->gateBetween;
            const bool between = std::find(gb.begin(), gb.end(), m_lastZoneBody) != gb.end() &&
                                 std::find(gb.begin(), gb.end(), body) != gb.end();
            if (between && !m_routeGatePassed.count(g.zone->id))
                fine(*g.zone, "пропущен обязательный створ");
            if (between)
                m_routeGatePassed.erase(g.zone->id);
        }
    }
    m_lastZoneBody = body;
}

void SpaceState::renderZoneViolationBanner(const Viewport& viewport)
{
    if (m_violationShowSeconds <= 0.0 || viewport.width <= 0)
        return;
    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const float w = text.measureTextPx(m_violationText, 16) + 40.0f;
    const float x = std::max(8.0f, (static_cast<float>(viewport.width) - w) * 0.5f);
    const float y = static_cast<float>(viewport.height) * 0.30f;
    text.solidRectPx(x, y, w, 32.0f, glm::vec4(0.35f, 0.02f, 0.02f, 0.9f));
    text.solidRectPx(x, y, w, 2.0f, glm::vec4(1.0f, 0.3f, 0.2f, 1.0f));
    text.textDrawPx(m_violationText, x + 20.0f, y + 22.0f, 16, glm::vec4(1.0f, 0.85f, 0.8f, 1.0f));
    text.endFrame();
}
