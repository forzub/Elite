// Route launch from the System Map (finish -> "official" / "unofficial") and the
// zone-violation watch. The planner is game::navigation::route (zones from
// assets/data/navigation/route_zones.json); the server's route autopilot flies
// the legs (GameSimulationRoute.cpp).
#include "src/game/SpaceStateInternal.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/game/navigation/route/RoutePlanner.h"
#include "src/game/simulation/JDrive.h"

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

void SpaceState::sendRouteControl(bool pause)
{
    if (!m_client)
        return;
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::RouteLeg;
    command.index = pause ? 3 : 4;
    msg.payload = command;
    m_client->sendMessage(msg);
    std::cerr << "[RoutePlan] " << (pause ? "PAUSE" : "RESUME") << " sent" << std::endl;
}

// The route window's plan -> planner points (ROUTE_PROGRAMS_DESIGN.md): each point's program decides
// its kind (dock stop / pass, orbit, landing), its actions the stop's action string ("refuel",
// "repair", "pause", joined by '+'). The J charge is walked along the points; where it would fall
// under 10 % of the capacity the planner inserts a refuel stop at the hub that adds the least way
// (marked "added by the planner" in the list) and plans again. launch = false: the summary only.
void SpaceState::launchPlannedRoute(bool official, bool launch)
{
    if (!m_client)
        return;
    const auto shipIt = m_client->world().ships().find(m_playerId.value);
    const auto* snapshot = m_client->celestialSnapshot();
    if (shipIt == m_client->world().ships().end() || !snapshot)
        return;
    const auto* system = m_client->starAtlas() ? m_client->starAtlas()->findSystem(snapshot->systemId) : nullptr;
    auto& plan = m_navigationWorkspace.routePlan();
    using P = game::navigation::NavigationWaypointProgram;
    using Anchor = game::navigation::NavigationRouteAnchorKind;
    namespace jd = game::simulation::jdrive;

    const auto& motion = shipIt->second.transform.motion;
    const double capacity = motion.jChargeCapacityT > 0.0 ? motion.jChargeCapacityT : jd::CobrettChargeCapacityT;
    // DEV: ELITE_DEV_ROUTE_CHARGE_T=<t> plans from that charge (planner refuel insertion check)
    static const double devCharge = [] {
        const char* v = std::getenv("ELITE_DEV_ROUTE_CHARGE_T");
        return v && *v ? std::atof(v) : -1.0;
    }();
    const double charge0 = devCharge >= 0.0 ? devCharge : motion.jChargeT >= 0.0 ? motion.jChargeT : capacity;
    constexpr double ShipMassT = 260.0;   // the Cobrett's dry mass (no cargo model on the client yet)
    const glm::dvec3 startM = world::coordinates::fullMeters(shipIt->second.transform.worldPosition);

    std::vector<std::string> inserted;
    route::PlanRequest req;
    route::PlannedRoute planned;
    double chargeAtFinish = charge0, chargeUsed = 0.0;
    std::string shortage;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        // intermediates in their order, then the finish
        std::vector<game::navigation::NavigationWaypoint*> order;
        for (auto& w : const_cast<std::vector<game::navigation::NavigationWaypoint>&>(plan.waypoints()))
            if (w.role == game::navigation::NavigationWaypointRole::Intermediate)
                order.push_back(&w);
        std::sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->sequence < b->sequence; });
        for (auto& w : const_cast<std::vector<game::navigation::NavigationWaypoint>&>(plan.waypoints()))
            if (w.role == game::navigation::NavigationWaypointRole::Finish)
                order.push_back(&w);
        if (order.empty() || order.back()->role != game::navigation::NavigationWaypointRole::Finish)
        {
            std::cerr << "[RoutePlan] no finish on the map" << std::endl;
            plan.summary = "нет финиша";
            return;
        }

        req = route::PlanRequest {};
        req.official = official;
        req.startM = startM;
        req.bodyAt = [&](const glm::dvec3& p) { return bodyAtPoint(*snapshot, system, p); };
        req.startBodyId = req.bodyAt(req.startM);
        req.hubsM.clear();
        for (const auto& [hubId, hub] : m_client->world().hubs())
            req.hubsM.emplace_back(hubId, world::coordinates::fullMeters(hub.worldPosition));

        for (const auto* w : order)
        {
            const bool finish = w->role == game::navigation::NavigationWaypointRole::Finish;
            const P program = game::navigation::effectiveProgram(*w);
            route::RoutePoint p;
            p.label = w->displayName;
            p.inserted = w->insertedByPlanner;
            p.worldM = world::coordinates::fullMeters(w->worldPosition);
            std::string act;
            const auto add = [&](const char* a) { act += act.empty() ? a : std::string("+") + a; };
            if (w->actions.refuel || w->stopAction == "refuel")
                add("refuel");
            if (w->actions.repair)
                add("repair");
            const bool landing = program == P::Land || program == P::LandAtSettlement;
            if (w->actions.pauseHere || (landing && !finish && !w->actions.takeOffAfter))
                add("pause");
            if (w->target.kind == Anchor::Hub && m_client->world().hubs().count(w->target.stableObjectId))
            {
                p.kind = route::RoutePoint::Kind::Hub;
                p.hubId = w->target.stableObjectId;
                p.worldM = world::coordinates::fullMeters(m_client->world().hubs().at(p.hubId).worldPosition);
                p.action = act;
                p.stop = program == P::Dock && (finish || !act.empty());
                if (!p.stop)
                    p.kind = route::RoutePoint::Kind::Point;   // a hub flown past
            }
            else if (w->target.kind == Anchor::Ship)
            {
                // a rendezvous: the server entity of the ship instance, its position now
                p.kind = route::RoutePoint::Kind::Ship;
                p.stop = true;
                p.action = act;
                for (const auto& [eid, s] : m_client->world().ships())
                    if (s.instanceId == w->target.shipInstanceId)
                    {
                        p.shipEntityId = eid;
                        p.worldM = world::coordinates::fullMeters(s.transform.worldPosition);
                    }
                if (p.shipEntityId == 0)
                {
                    std::cerr << "[RoutePlan] rendezvous target " << w->target.shipInstanceId << " not in this system - skipped" << std::endl;
                    continue;
                }
            }
            else if (landing)
            {
                p.kind = route::RoutePoint::Kind::Land;
                p.stop = true;
                p.action = act;
                p.bodyId = w->target.stableObjectId;
                if (program == P::LandAtSettlement)
                    p.landingSpec = !w->actions.landingSite.empty() ? w->actions.landingSite
                                  : w->sourceObjectId.rfind("site:", 0) == 0 ? w->sourceObjectId : "site:" + w->target.stableObjectId;
                for (const auto& b : snapshot->bodies)
                    if (b.id == p.bodyId)
                        p.worldM = b.worldMeters;
            }
            else if (w->target.kind == Anchor::CelestialBody && program != P::PassThrough &&
                     (finish || program == P::Orbit || program == P::OrbitSurvey))
            {
                p.kind = route::RoutePoint::Kind::Orbit;
                p.bodyId = w->target.stableObjectId;
                p.stop = true;
                p.action = act;
                for (const auto& b : snapshot->bodies)
                    if (b.id == p.bodyId)
                    {
                        p.worldM = b.worldMeters;
                        // a low, safe orbit: 6 % of the radius, at least 100 km; the point's own altitude when set
                        p.orbitAltitudeM = w->actions.orbitAltitudeM > 0.0 ? w->actions.orbitAltitudeM
                                                                           : std::max(100000.0, b.radiusKm * 1000.0 * 0.06);
                    }
            }
            else
            {
                p.kind = route::RoutePoint::Kind::Point;
                p.stop = finish || program == P::Stop || !act.empty();
                p.action = act;
            }
            req.points.push_back(p);
        }

        // the J charge along the points (straight segments; a refuel stop fills the tank)
        double charge = charge0;
        chargeUsed = 0.0;
        int failAt = -1;
        glm::dvec3 cur = startM;
        for (std::size_t k = 0; k < req.points.size(); ++k)
        {
            const double cost = route::estimateLegChargeT(glm::length(req.points[k].worldM - cur), ShipMassT);
            charge -= cost;
            chargeUsed += cost;
            const bool refuelHere = req.points[k].kind == route::RoutePoint::Kind::Hub && req.points[k].stop &&
                                    req.points[k].action.find("refuel") != std::string::npos;
            // a 10 % reserve on arrival; a refuel stop only has to be reached
            if ((refuelHere ? charge < 0.0 : charge < 0.1 * capacity) && failAt < 0)
                failAt = static_cast<int>(k);
            if (refuelHere)
                charge = capacity;
            cur = req.points[k].worldM;
        }
        chargeAtFinish = charge;
        if (failAt < 0)
            break;
        // insert a refuel stop before the failing point: the hub adding the least way
        const glm::dvec3 a = failAt == 0 ? startM : req.points[static_cast<std::size_t>(failAt - 1)].worldM;
        const glm::dvec3 b = req.points[static_cast<std::size_t>(failAt)].worldM;
        std::string bestHub;
        double best = 1.0e300;
        for (const auto& [hubId, p] : req.hubsM)
        {
            const double extra = glm::length(p - a) + glm::length(b - p) - glm::length(b - a);
            if (extra < best)
            {
                best = extra;
                bestHub = hubId;
            }
        }
        if (bestHub.empty())
        {
            shortage = "заряда J не хватит, заправки на пути нет";
            break;
        }
        game::navigation::RouteTargetRef target;
        target.kind = Anchor::Hub;
        target.systemId = snapshot->systemId;
        target.stableObjectId = bestHub;
        const auto& hub = m_client->world().hubs().at(bestHub);
        auto& w = plan.rememberCandidate(target, "planner:refuel:" + bestHub, hub.worldPosition, std::string(), hub.name);
        if (w.role != game::navigation::NavigationWaypointRole::Intermediate)
            plan.toggleRole(w.id, game::navigation::NavigationWaypointRole::Intermediate);
        w.program = P::Dock;
        w.actions.refuel = true;
        w.stopAction = "refuel";
        w.insertedByPlanner = true;
        w.insertedReason = "заправка";
        plan.moveIntermediateWaypoint(w.id, failAt + 1);
        inserted.push_back(hub.name);
        std::cerr << "[RoutePlan] J charge short before point " << failAt + 1 << ": planner inserted a refuel stop at " << bestHub
                  << " (+" << best / 1000.0 << " km)" << std::endl;
    }

    const auto zones = route::resolveZones(zoneCatalog(), snapshot->systemId, routeAnchorResolver());
    planned = route::planRoute(req, zones, zoneCatalog().hubRadarM);
    std::cerr << "[RoutePlan] " << (official ? "OFFICIAL" : "UNOFFICIAL") << (launch ? "" : " (calculate only)") << " route: "
              << planned.legs.size() << " legs, " << planned.lengthM / 1000.0 << " km, J charge used ~" << chargeUsed << " t, at the finish "
              << chargeAtFinish << " / " << capacity << " t";
    for (const auto& n : planned.notes)
        std::cerr << " | " << n;
    std::cerr << std::endl;
    {
        char text[256];
        std::snprintf(text, sizeof text, "%zu плеч · %.0f тыс. км · J −%.3f т · финиш %.2f/%.0f т", planned.legs.size(),
                      planned.lengthM / 1.0e6, chargeUsed, std::max(0.0, chargeAtFinish), capacity);
        plan.summary = text;
        if (!inserted.empty())
            plan.summary += " · +" + std::to_string(inserted.size()) + " запр.";
        if (!shortage.empty())
            plan.summary += " · " + shortage;
    }

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
    if (!launch || !shortage.empty())
        return;

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
    m_navigationWorkspace.routePlan().serverRouteState = shipIt->second.transform.motion.routeState;
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
