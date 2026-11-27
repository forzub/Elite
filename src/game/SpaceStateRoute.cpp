// Route launch from the System Map (finish -> "official" / "unofficial") and the
// zone-violation watch. The planner is game::navigation::route (zones from
// assets/data/navigation/route_zones.json); the server's route autopilot flies
// the legs (GameSimulationRoute.cpp).
#include "src/world/WorldLaws.h"
#include "src/game/localization/LocalizationService.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/fuel/FuelStationCatalog.h"
#include "src/game/SpaceStateInternal.h"
#include "src/game/navigation/rules/CapsuleRules.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/game/navigation/route/RoutePlanner.h"
#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"
#include "src/game/navigation/rules/JDrive.h"
#include "src/game/navigation/rules/InterstellarJumpRoute.h"
#include "src/game/navigation/rules/SensorSpeedRules.h"
#include "src/game/navigation/rules/JumpSumps.h"
#include "src/game/system_map/PlanetBodyOrientation.h"
#include "src/world/celestial/StarAtlasDatabase.h"

namespace l10n = game::localization;

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
    // ONE rule with the server (BodyFlightRules.h)
    using B = world::celestial::CelestialBodyState;
    const std::size_t k = game::navigation::rules::dominantBody(
        snapshot.bodies, p, [](const B& b) -> const std::string& { return b.id; }, [](const B& b) { return b.parentId; },
        [&](const B& b) { return mu(b.id); }, [](const B& b) { return b.worldMeters; });
    return k < snapshot.bodies.size() ? snapshot.bodies[k].id : std::string();
}

const route::ZoneCatalog& zoneCatalog()
{
    static const route::ZoneCatalog catalog = route::loadZoneCatalog();
    return catalog;
}

// the route's player-visible text in the active UI language (cockpit.route.* in
// src/assets/localization/ui/cockpit/flight.json)
std::string routeText(const Application* app, const char* key, std::initializer_list<std::string> args = {})
{
    return game::presentation::TerminalLocalizedText::render(
        [app](const char* k) { return localizedUiText(app, k, k); }, key, args);
}

std::string routeNumber(const char* format, double value)
{
    char b[48];
    std::snprintf(b, sizeof b, format, value);
    return b;
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
    namespace jd = game::navigation::jdrive;

    const auto& motion = shipIt->second.transform.motion;
    const double capacity = motion.jChargeCapacityT;   // the ship's own (0: no jump drive)
    if (capacity <= 0.0 || motion.jumpCoreRatingT <= 0.0)
    {
        plan.summary = routeText(context().app, "cockpit.route.no_jump_drive");
        std::cerr << "[RoutePlan] the ship has no jump drive - no J route" << std::endl;
        return;
    }
    // DEV: ELITE_DEV_ROUTE_CHARGE_T=<t> plans from that charge (planner refuel insertion check)
    static const double devCharge = [] {
        const char* v = std::getenv("ELITE_DEV_ROUTE_CHARGE_T");
        return v && *v ? std::atof(v) : -1.0;
    }();
    const double charge0 = devCharge >= 0.0 ? devCharge : motion.jChargeT >= 0.0 ? motion.jChargeT : capacity;
    const double shipMassT = motion.shipMassT;        // replicated from the ship's parameters
    const double coreRatingT = motion.jumpCoreRatingT;
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
            plan.summary = routeText(context().app, "cockpit.route.no_finish");
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
        // the bodies to go round: the star (the largest mu) by 20 radii, the rest by 150 km / 5 %
        {
            const auto mu = [&](const std::string& id) {
                if (system)
                    for (const auto& b : system->bodies)
                        if (b.id == id)
                            return b.gravitationalParameterM3s2;
                return 0.0;
            };
            double starMu = 0.0;
            for (const auto& b : snapshot->bodies)
                starMu = std::max(starMu, mu(b.id));
            req.bodies.clear();
            const auto atmosphere = [&](const std::string& id, double r) {
                if (system)
                    for (const auto& b : system->bodies)
                        if (b.id == id)
                            return std::max(r, b.atmosphereOuterRadiusKm * 1000.0);
                return r;
            };
            for (const auto& b : snapshot->bodies)
            {
                const double r = b.radiusKm * 1000.0;
                if (r < 50000.0)
                    continue;
                const bool star = starMu > 0.0 && mu(b.id) >= starMu;
                std::string name = b.id.substr(b.id.rfind('.') + 1);
                req.bodies.push_back({star ? routeText(context().app, "cockpit.route.star_heat", {name}) : name, b.worldMeters,
                                      game::navigation::rules::clearanceM(r, atmosphere(b.id, r), star)});
            }
        }
        // the scheduled network's hub-to-hub routes as green corridors round the bodies
        req.corridors.clear();
        if (const auto* sites = world::celestial::settlements::settlementCatalog())
            for (const auto& r : sites->routesInSystem(snapshot->systemId))
            {
                const auto a = m_client->world().hubs().find(r.origin);
                const auto b = m_client->world().hubs().find(r.destination);
                if (a == m_client->world().hubs().end() || b == m_client->world().hubs().end() || r.origin == r.destination)
                    continue;
                route::CorridorPath c;
                c.name = a->second.name + " – " + b->second.name;
                c.points = route::corridorPath(world::coordinates::fullMeters(a->second.worldPosition),
                                               world::coordinates::fullMeters(b->second.worldPosition), req.bodies);
                req.corridors.push_back(std::move(c));
            }

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
            else if (w->target.kind == Anchor::Hub)
            {
                // a catalog (world-site) hub - no runtime hub: its marker position, a stop there
                p.kind = route::RoutePoint::Kind::Point;
                p.stop = true;
                p.action = act;
                if (glm::length(p.worldM) < 1.0)
                {
                    std::cerr << "[RoutePlan] no position for the hub " << w->target.stableObjectId << std::endl;
                    plan.summary = routeText(context().app, "cockpit.route.no_target_coords", {w->displayName});
                    return;
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
            const double cost = route::estimateLegChargeT(glm::length(req.points[k].worldM - cur), shipMassT, coreRatingT);
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
            if (hubId == req.points[static_cast<std::size_t>(failAt)].hubId)
                continue;   // the point itself: it is a stop already (a refuel there needs its own action)
            if (!game::fuel::FuelStationCatalog::instance().sellsFuel(hubId))
                continue;   // only a station that sells jump charge (docs/FUEL_STATIONS.md)
            const double extra = glm::length(p - a) + glm::length(b - p) - glm::length(b - a);
            if (extra < best)
            {
                best = extra;
                bestHub = hubId;
            }
        }
        if (bestHub.empty())
        {
            shortage = routeText(context().app, "cockpit.route.charge_short");
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
        w.insertedReason = routeText(context().app, "cockpit.route.refuel_reason");
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
        plan.summary = routeText(context().app, "cockpit.route.summary",
                                 {std::to_string(planned.legs.size()), routeNumber("%.0f", planned.lengthM / 1.0e6), routeNumber("%.3f", chargeUsed),
                                  routeNumber("%.2f", std::max(0.0, chargeAtFinish)), routeNumber("%.0f", capacity)});
        if (!inserted.empty())
            plan.summary += " · " + routeText(context().app, "cockpit.route.refuel_count", {std::to_string(inserted.size())});
        if (!shortage.empty())
            plan.summary += " · " + shortage;
        int outside = 0;
        for (const auto& l : planned.legs)
            if (rescueAt(l.worldM, snapshot->systemId).zone != game::navigation::rescue::Zone::Inside)
                ++outside;
        if (outside > 0)
        {
            plan.summary += " · " + routeText(context().app, "cockpit.route.outside_rescue",
                                              {std::to_string(outside), std::to_string(planned.legs.size())});
            std::cerr << "[RoutePlan] " << outside << " of " << planned.legs.size() << " points outside the rescue zones" << std::endl;
        }
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
    // START follows the calculation: enabled while the route is still the one planned here
    plan.calculatedSignature = shortage.empty() ? plan.planSignature() : std::string();
    if (!launch || !shortage.empty())
        return;

    // the client flies it (SpaceStateRouteRun.cpp, NAVIGATION_V2 §1)
    startClientRoute(planned.legs);
    m_routeOfficial = official;
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
    {
        // the route state for the window; a flown route that ended = "МАРШРУТ ВЫПОЛНЕН" (author:
        // "карточка должна была ... сообщить о том, что мишн комплит")
        auto& plan = m_navigationWorkspace.routePlan();
        const int now = clientRouteState();
        if (plan.serverRouteState != 0 && now == 0 && m_routeLaunched)
        {
            plan.summary = routeText(context().app, "cockpit.route.complete");
            m_routeLaunched = false;
            std::cerr << "[RoutePlan] ROUTE COMPLETE" << std::endl;
        }
        if (now != 0)
            m_routeLaunched = true;
        plan.serverRouteState = now;
    }
    // the violations are judged by the server (GameSimulationZones.cpp): show its latest one
    const auto& motion = shipIt->second.transform.motion;
    if (motion.zoneViolationSerial != m_lastZoneViolationSerial)
    {
        const bool first = m_lastZoneViolationSerial == 0xFFFF;
        m_lastZoneViolationSerial = motion.zoneViolationSerial;
        if (!first && motion.zoneViolationKind != 0)
            for (const auto& z : zoneCatalog().zones)
                if (z.id == motion.zoneViolationZoneId)
                {
                    m_violationText = routeText(
                        context().app, "cockpit.route.violation",
                        {routeText(context().app, motion.zoneViolationKind == 1 ? "cockpit.route.violation_red_zone" : "cockpit.route.violation_gate"),
                         z.name, std::to_string(static_cast<long long>(z.fineCr)), z.authority});
                    m_violationShowSeconds = 8.0;
                }
    }
    // hub traffic control (GameSimulationHubSecurity.cpp, docs/HUB_SECURITY.md): the controller's warning, the fines
    // (banked at the hub polity's bank; the part booked as debt), the fire-control lock, the incident closed
    if (motion.securitySerial != m_lastSecuritySerial)
    {
        const bool first = m_lastSecuritySerial == 0xFFFF;
        m_lastSecuritySerial = motion.securitySerial;
        const std::string hub = motion.securityHubId;
        const std::string lane = motion.securityLaneId;
        std::string text;
        switch (first ? 0 : motion.securityEvent)
        {
        case 1: text = routeText(context().app, "cockpit.security.warning", {hub, lane}); break;
        case 2:
            text = routeText(context().app, "cockpit.security.fine",
                             {hub, std::to_string(static_cast<long long>(motion.securityEventFineCr)), lane,
                              motion.securityEventDebtCr > 0.5
                                  ? routeText(context().app, "cockpit.security.debt",
                                              {std::to_string(static_cast<long long>(motion.securityEventDebtCr))})
                                  : std::string()});
            break;
        case 3: text = routeText(context().app, "cockpit.security.lock", {hub}); break;
        case 4: text = routeText(context().app, "cockpit.security.clear", {hub}); break;
        default: break;
        }
        if (!text.empty())
        {
            m_violationText = text;
            m_violationShowSeconds = 8.0;
            std::cerr << "[Corridor] client notice: " << text << std::endl;
        }
    }
    m_finesTotalCr = motion.zoneFinesTotalCr + motion.securityFinesTotalCr;
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

// The pilot is dead (author 2026-11-22: «врезание в здание не дало никакого эффекта, игра по-идее должна была
// закончиться»): the player's ship is wrecked / disabled (destroyedCause) and its cockpit is gone (the crew lost with it)
// -> the whole screen says so and the flight controls are over (handleInput); the menu (Esc) stays, and the DEV restore
// (Ctrl+Alt+R) for the tests. A crew that escaped in the EC-2 cockpit is not dead (the capsule states show it).
bool SpaceState::playerPilotLost(std::string* cause) const
{
    if (!m_client)
        return false;
    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
        return false;
    const auto& ship = it->second;
    const auto& motion = ship.transform.motion;
    if (motion.destroyedCause == 0)
        return false;
    bool cockpitLost = false;
    for (const auto& m : ship.modules)
        if (m.moduleId == "ship_cockpit")
            cockpitLost = m.health <= 0.0f;
    namespace cap = game::navigation::capsule;
    const auto capsule = static_cast<cap::State>(motion.capsuleState);
    // the pilot alive in the cockpit-capsule is NOT lost (the capsule flies on); the capsule itself destroyed is
    const bool killed = capsule != cap::State::PilotAlive &&
                        (cockpitLost || capsule == cap::State::PilotKilled || capsule == cap::State::Destroyed ||
                         motion.destroyedCause == 4);   // crushed against a structure
    if (!killed)
        return false;
    if (cause)
        *cause = motion.destroyedCause == 1 ? l10n::tr("cockpit.pilot_lost.cause.surface_impact", "surface impact")
               : motion.destroyedCause == 4 ? l10n::tr("cockpit.pilot_lost.cause.structure_collision", "collision with a structure")
               : capsule == cap::State::PilotKilled ? l10n::tr("cockpit.pilot_lost.cause.impact_g", "g-load on impact")
               : capsule == cap::State::Destroyed || motion.destroyedCause == 3 ? l10n::tr("cockpit.pilot_lost.cause.cockpit_destroyed", "cockpit destroyed")
                                            : l10n::tr("cockpit.pilot_lost.cause.g_load", "g-load");
    return true;
}

void SpaceState::renderPilotLostOverlay(const Viewport& viewport)
{
    std::string cause;
    static bool shown = false;
    if (viewport.width <= 0 || !playerPilotLost(&cause))
    {
        shown = false;
        return;
    }
    if (!shown)
    {
        shown = true;
        std::cerr << "[PilotLost] the pilot-lost screen is up: " << cause << std::endl;
    }
    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const float W = static_cast<float>(viewport.width), H = static_cast<float>(viewport.height);
    // the cockpit camera is gone with the cockpit: WHITE NOISE on the front view (author 2026-11-22: «если кабина
    // разрушается, можно выводить на фронтальной камере белый шум. хоть так будет понятно, что что-то произошло») - the
    // other cameras (rear, bottom, drone) still see the wreck
    if (m_activeCameraMode == ShipCameraMode::Cockpit)
    {
        static std::uint32_t seed = 0x9E3779B9u;
        const auto next = [&]() {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            return seed;
        };
        constexpr float Cell = 8.0f;
        text.solidRectPx(0.0f, 0.0f, W, H, glm::vec4(0.12f, 0.12f, 0.12f, 1.0f));
        for (float y = 0.0f; y < H; y += Cell)
        {
            // a rolling band now and then, as a dead feed does
            const float band = (next() & 31u) == 0u ? 0.25f : 0.0f;
            for (float x = 0.0f; x < W; x += Cell)
            {
                const float g = static_cast<float>(next() & 255u) / 255.0f;
                if (g < 0.35f)
                    continue;
                text.solidRectPx(x, y, Cell, Cell, glm::vec4(glm::vec3(std::min(1.0f, g * 0.85f + band)), 1.0f));
            }
        }
    }
    text.solidRectPx(0.0f, 0.0f, W, H, glm::vec4(0.0f, 0.0f, 0.0f, m_activeCameraMode == ShipCameraMode::Cockpit ? 0.35f : 0.72f));
    const std::string title = l10n::tr("cockpit.pilot_lost.title", "PILOT KILLED");
    const std::string line = l10n::tr("cockpit.pilot_lost.line", {{"cause", cause}}, "Ship wrecked: {cause}. The crew died with the cockpit.");
    const std::string hint = l10n::tr("cockpit.pilot_lost.hint", "Esc - menu");
    const float tw = text.measureTextPx(title, 40);
    text.solidRectPx(0.0f, H * 0.38f, W, 2.0f, glm::vec4(0.9f, 0.15f, 0.1f, 0.9f));
    text.textDrawPx(title, 0.5f * (W - tw), H * 0.38f + 58.0f, 40, glm::vec4(1.0f, 0.25f, 0.2f, 1.0f));
    text.textDrawPx(line, 0.5f * (W - text.measureTextPx(line, 18)), H * 0.38f + 100.0f, 18, glm::vec4(0.95f, 0.9f, 0.88f, 1.0f));
    text.textDrawPx(hint, 0.5f * (W - text.measureTextPx(hint, 14)), H * 0.38f + 132.0f, 14, glm::vec4(0.7f, 0.72f, 0.76f, 0.9f));
    text.solidRectPx(0.0f, H * 0.38f + 150.0f, W, 2.0f, glm::vec4(0.9f, 0.15f, 0.1f, 0.9f));
    text.endFrame();
}

// Sensor-limited speed (docs/SENSOR_LIMITED_SPEED.md, rules/SensorSpeedRules.h): the charted test from data the client
// already has - the green corridors and the hub radar zone of route_zones.json, the catalogue system's surveyed shell
// (its farthest body now x SurveyShellFactor), the lane records the ship carries (a system's approaches). Uncharted:
// the radar range of the hull's passport (no radar: optics) and the reaction (a failed nav computer: longer) go into
// the sample; the route pilot / J autopilot bound their speed with them. Manual flight is not touched.
// Variant 1 (author 2026-11-20): uncharted space is bound only in / near a hazard the client already has data for - the
// system's generated belts / debris disks / trojans (JumpSumps), the planets' rings, the space near a body, a deep-space
// place's swarm; "near" = within the stopping run at the current speed (rules::sensor::inHazard).
void SpaceState::applySensorSpeedInputs(game::navigation::globalflight::BodyAutopilotShip& sample)
{
    namespace sn = game::navigation::rules::sensor;
    namespace jr = game::navigation::jump;
    namespace jd = game::navigation::jdrive;
    using Atlas = world::celestial::StarAtlasDatabase;
    sample.sensorRangeM = 0.0;
    sample.sensorReactionS = 0.0;
    sample.sensorHazardDistanceM = 0.0;
    m_sensorHazardKind = 0;
    if (!m_client)
        return;
    const auto shipIt = m_client->world().ships().find(m_playerId.value);
    if (shipIt == m_client->world().ships().end() || !shipIt->second.descriptor)
        return;
    const auto& motion = shipIt->second.transform.motion;
    const int systemId = motion.systemId;
    const glm::dvec3 p = sample.positionM;
    const auto* atlas = m_client->starAtlas();
    sn::ChartedQuery q;
    const auto* summary = atlas ? atlas->findSystemSummary(systemId) : nullptr;
    q.catalogueSystem = summary && !Atlas::isDeepSpace(systemId) && summary->catalogScope == world::celestial::StarSystemCatalogScope::Local;
    // the star: the heaviest body; the shell: the farthest body from it
    const game::navigation::GravityBody* star = nullptr;
    for (const auto& b : m_bodyAutopilotBodies)
        if (!star || b.gravitationalParameterM3s2 > star->gravitationalParameterM3s2)
            star = &b;
    if (star)
    {
        q.distanceFromStarM = glm::length(p - star->centerMeters);
        for (const auto& b : m_bodyAutopilotBodies)
            q.farthestBodyM = std::max(q.farthestBodyM, glm::length(b.centerMeters - star->centerMeters));
    }
    for (const auto& [hubId, hub] : m_client->world().hubs())
        if (hub.systemId == systemId)
        {
            q.nearestHubM = std::min(q.nearestHubM, glm::length(world::coordinates::fullMeters(hub.worldPosition) - p));
            q.inhabited = true;
        }
    q.inhabited = q.inhabited || jr::systemHasBeacons(systemId);   // world places = beacons (JumpSumps.h)
    static const bool devNoBeacons = std::getenv("ELITE_DEV_NO_BEACONS") != nullptr;
    if (devNoBeacons)
        q.inhabited = false;   // DEV: no system has beacons -> none counts as surveyed (its shell); hub zones / corridors stay
    q.hubZoneM = zoneCatalog().hubRadarM;
    for (const auto& z : route::resolveZones(zoneCatalog(), systemId, routeAnchorResolver()))
        if (z.zone && z.zone->colour == route::ZoneColour::Green && z.contains(p))
            q.inGreenCorridor = true;
    // the lane records live in the nav computer (JUMP_MECHANICS 10k): a failed one reads none
    const bool navDown = motion.navComputerState == 2;
    if (atlas && !navDown && Atlas::isApproach(systemId))
    {
        const int sys = Atlas::systemOfApproach(systemId);
        if (const auto* shell = jr::approachShellOf(*atlas, sys))
            for (const auto& r : jr::parseLaneRecords(motion.laneRecords))
                if (r.systemId == sys && jr::laneState(*shell, r, m_client->lastSimulationMetadata().universeTimeSeconds) != jr::LaneState::Unknown)
                    q.approachLaneRecord = true;
    }
    const sn::ChartedBy by = sn::chartedBy(q);
    // the passport radar (a catalogue hull), else the installed default radar (the legacy descriptors)
    const auto& desc = *shipIt->second.descriptor;
    const double range = sn::sensorRangeM(desc.catalog.radarRangeMeters > 0.0 ? desc.catalog.radarRangeMeters
                                          : desc.defaultEquipment.radar ? desc.defaultEquipment.radar->maxRange : 0.0);
    m_sensorShownRangeM = range;
    // uncharted: the hazards the client has data for; the bound only in / near the nearest one
    std::string hazardLog = "-";
    if (by == sn::ChartedBy::None)
    {
        std::vector<sn::HazardZone> zones;
        const auto& bodies = m_bodyAutopilotSnapshot.bodies;   // the same universe time as the sample (clientBodyAutopilotWorld)
        if (const auto* system = atlas ? atlas->findSystem(systemId) : nullptr)
        {
            std::unordered_map<std::string, glm::dvec3> centres;
            for (const auto& b : bodies)
                centres[b.id] = b.worldMeters;
            for (const auto& s : jr::jumpSumpsOfSystem(*system))
            {
                const auto c = centres.find(s.centreBodyId);
                if (c == centres.end())
                    continue;
                sn::HazardZone z;
                z.kind = sn::HazardKind::Belt;
                z.name = s.name;
                z.centreM = c->second;
                z.normal = glm::dvec3(0.0, 1.0, 0.0);   // the system frame's x-z plane (JumpSumps.h)
                z.innerM = s.innerM;
                z.outerM = s.outerM;
                z.halfThicknessM = sn::BeltHeightSigmas * s.halfThicknessM;
                if (s.kind == "trojans")
                {
                    const auto planet = centres.find(s.planetId);
                    if (planet == centres.end())
                        continue;
                    glm::dvec3 pl = planet->second - c->second;
                    pl.y = 0.0;
                    if (glm::length(pl) <= 0.0)
                        continue;
                    pl = glm::normalize(pl);
                    z.arcHalfRad = sn::TrojanHalfArcRad;
                    for (const double lagrange : {1.0471975511965976, -1.0471975511965976})   // L4 / L5: +-60 deg
                    {
                        // turned about +y in the x-z plane the way atan2(z, x) grows (JumpSumps' azimuth)
                        z.arcCentreDir = glm::dvec3(pl.x * std::cos(lagrange) - pl.z * std::sin(lagrange), 0.0,
                                                    pl.x * std::sin(lagrange) + pl.z * std::cos(lagrange));
                        zones.push_back(z);
                    }
                    continue;
                }
                zones.push_back(z);
            }
        }
        for (const auto& b : bodies)
        {
            if (b.rings.empty())
                continue;
            sn::HazardZone z;
            z.kind = sn::HazardKind::Ring;
            z.name = b.name.empty() ? b.id : b.name;
            z.centreM = b.worldMeters;
            z.normal = glm::dvec3(game::system_map::makePlanetTextureOrientation(b.axialTiltDeg, b.axisNodeDeg, 0.0, 0.0).north);
            z.innerM = std::numeric_limits<double>::infinity();
            for (const auto& r : b.rings)
            {
                z.innerM = std::min(z.innerM, r.innerRadiusKm * 1000.0);
                z.outerM = std::max(z.outerM, r.outerRadiusKm * 1000.0);
            }
            z.halfThicknessM = sn::RingHalfThicknessM;
            if (glm::length(z.normal) > 0.5 && z.outerM > z.innerM)
                zones.push_back(z);
        }
        for (const auto& g : m_bodyAutopilotBodies)
        {
            sn::HazardZone z;
            z.kind = sn::HazardKind::Body;
            z.name = g.id;
            for (const auto& b : bodies)
                if (b.id == g.id && !b.name.empty())
                    z.name = b.name;
            z.centreM = g.centerMeters;
            z.outerM = std::max(sn::NearBodyRadii * g.radiusMeters, g.atmosphereRadiusMeters);
            zones.push_back(z);
        }
        // a deep-space place: the frame is centred on its swarm (StarAtlasDatabase::buildDeepSpacePlaces)
        if (const auto* o = atlas && Atlas::isDeepSpace(systemId) ? atlas->deepSpaceObject(systemId) : nullptr)
        {
            constexpr double LyM = 9.4607304725808e15;
            double radiusM = 0.0;
            const auto& props = o->properties;
            if (props.contains("geometry") && props["geometry"].contains("radii_ly") && props["geometry"]["radii_ly"].is_array())
                for (const auto& r : props["geometry"]["radii_ly"])
                    if (r.is_number())
                        radiusM = std::max(radiusM, r.get<double>() * LyM);   // the ellipsoid's longest semi-axis (conservative)
            if (props.contains("extent_au") && props["extent_au"].is_number())
                radiusM = std::max(radiusM, props["extent_au"].get<double>() * 1.495978707e11);
            if (radiusM > 0.0)
            {
                sn::HazardZone z;
                z.kind = sn::HazardKind::DebrisField;
                z.name = o->name.empty() ? o->id : o->name;
                z.outerM = radiusM;
                zones.push_back(z);
            }
        }
        double distance = 0.0;
        const int k = sn::nearestHazard(zones, p, distance);
        // the margin: the stopping run at the speed now - the reaction plus the field's ramp to full, then the planned braking
        const double rampLeftS = jd::DriveRampDoublingS * std::log2(std::max(1.0, jd::MaxAccelMps2 / std::max(::world::laws::StandardGravityMps2, sample.jFieldLevelMps2)));
        const double delayS = sn::reactionSeconds(navDown) + rampLeftS;
        const double brake = sn::HazardBrakeFraction * jd::MaxAccelMps2;
        const double speed = glm::length(sample.velocityMps);
        if (k >= 0 && sn::inHazard(distance, speed, delayS, brake))
        {
            const auto& z = zones[static_cast<std::size_t>(k)];
            sample.sensorRangeM = range;
            sample.sensorReactionS = sn::reactionSeconds(navDown);
            sample.sensorHazardDistanceM = distance;
            m_sensorHazardKind = static_cast<int>(z.kind);
            m_sensorHazardName = z.name;
            m_sensorHazardDistanceM = distance;
            static const char* kinds[] = {"-", "belt", "rings", "near body", "debris field"};
            hazardLog = std::string(kinds[static_cast<int>(z.kind)]) + " " + z.name + (distance > 0.0 ? " AHEAD" : " INSIDE");
        }
        else if (k >= 0)
            hazardLog = "clear (nearest " + zones[static_cast<std::size_t>(k)].name + ")";
        else
            hazardLog = "clear (no hazard data)";
        if (hazardLog != m_sensorHazardLogged)
        {
            std::cerr << "[SensorSpeed] hazard: " << hazardLog << ", distance " << distance / 1000.0 << " km, speed " << speed / 1000.0
                      << " km/s, stopping run " << sn::stoppingDistanceM(speed, delayS, brake) / 1000.0 << " km" << std::endl;
            m_sensorHazardLogged = hazardLog;
        }
    }
    else
        m_sensorHazardLogged.clear();
    if (static_cast<int>(by) != m_sensorChartedBy)
    {
        static const char* reasons[] = {"UNCHARTED - speed by the sensors in / near a hazard", "a green corridor", "a hub radar zone", "the system's surveyed shell",
                                        "a lane record of the approaches"};
        std::cerr << "[SensorSpeed] system " << systemId << ": " << reasons[static_cast<int>(by)] << " (star " << q.distanceFromStarM / 1.495978707e11
                  << " AU, shell " << sn::SurveyShellFactor * q.farthestBodyM / 1.495978707e11 << " AU, nearest hub " << q.nearestHubM / 1000.0
                  << " km); hull " << desc.catalog.passportId << ", radar " << range / 1000.0 << " km, reaction " << sn::reactionSeconds(navDown) << " s, v_max at full J "
                  << sn::sensorSpeedLimitMps(range, sn::reactionSeconds(navDown), game::navigation::jdrive::MaxAccelMps2) / 1000.0 << " km/s"
                  << std::endl;
        m_sensorChartedBy = static_cast<int>(by);
    }
}
