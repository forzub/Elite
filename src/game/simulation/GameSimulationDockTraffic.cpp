#include "src/game/economy/EconomyService.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/simulation/GameSimulationInternal.h"

#include <cstdio>
#include <cstdlib>

#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/ship/descriptors/OrbitalCargoShuttleDescriptor.h"
#include "src/game/traffic/DockServiceTraffic.h"
#include "src/world/celestial/WorldSitesCatalog.h"

#include <algorithm>
#include <cctype>

// Earth Orbital Hub dock traffic: the deterministic DockServiceTraffic model
// (dispatcher + cargo shuttles + busy-traffic Cobretts) realised as
// TrafficShell ships. Poses come in the dock FIXED frame: origin = the hex
// dock centre, basis = the fixed service block (the dock's base rotation
// without spin); the dock phase is the authoritative module spin angle.
//
// ELITE_DOCK_TRAFFIC=0 disables it, ELITE_DOCK_TRAFFIC_COBRETTS=<n>,
// ELITE_DOCK_TRAFFIC_INTERVAL=<s> tune it, ELITE_DOCK_TRAFFIC_LOG=<path>
// writes the traffic / dispatcher log.

// GENERAL FOR EVERY HEX DOCK (author 2026-11-17: «давай переделаем механизм захвата дока - сделаем его общим для всех
// доков»): every HubHexDock static object of the active system is a dock SITE (m_dockSites, keyed by its hub module
// id) with its own DockServiceTraffic (dispatcher, pads, capture, NPC traffic); its fixed frame is the nearest
// HubDockServiceBlock of the same hub. A pilot ship belongs to one site at a time (m_pilotDockSite): the dock it asked
// (ClientShipCommand::RequestDockArrival target = the dock module id) or whose approach zone it entered.

namespace
{

int envInt(const char* name, int fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? std::atoi(v) : fallback;
}
double envDouble(const char* name, double fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? std::atof(v) : fallback;
}
// The pilot's hold is reached: within this distance of the hold point and
// slower than this relative to the dock.
constexpr double HoldReachedMeters = 60.0;
constexpr double HoldReachedMps = 2.5;
// A request is dropped when the ship is this far from the dock (the pilot's
// automatic route starts up to ~7 km away, through the guidance cylinder).
constexpr double RequestDropMeters = 20000.0;
} // namespace

game::traffic::dock::DockServiceTraffic* GameSimulation::dockServiceTraffic() noexcept
{
    // the first dock site (diagnostics / the stress harness)
    for (auto& [id, site] : m_dockSites)
        if (site.traffic)
            return site.traffic.get();
    return nullptr;
}

bool GameSimulation::removeTrafficShell(EntityId id)
{
    const auto it = m_shipShells.find(id);
    if (it == m_shipShells.end())
        return false;
    m_shipShells.erase(it);
    forgetShip(id);   // the id leaves the simulation: every side table
    return true;
}

const std::vector<std::pair<const StaticObject*, const StaticObject*>>& GameSimulation::hexDockPairs() const
{
    // Cached (live 2026-11-19: with 293 cities the old O(docks x static objects) scan, several times a tick, cost
    // ~12 ms of a 20 ms tick - the game fell into catch-up batches at ~5 FPS and the docking autopilot lost its
    // corridor). Static objects are only ever added (spawnStation) and an unordered_map keeps its elements' addresses
    // on insertion, so the pairs stay valid until the count changes.
    if (m_hexDockPairsCacheValid && m_hexDockPairsCacheGeneration == m_staticObjectsGeneration)
        return m_hexDockPairsCache;
    auto& out = m_hexDockPairsCache;
    out.clear();
    std::unordered_map<std::string, std::vector<const StaticObject*>> serviceBlocksByHub;
    for (const auto& [bid, b] : m_staticObjects)
        if (b.attachedToHub && b.type == ObjectType::HubDockServiceBlock)
            serviceBlocksByHub[b.hubId].push_back(&b);
    for (const auto& [id, obj] : m_staticObjects)
    {
        if (!obj.attachedToHub || obj.type != ObjectType::HubHexDock)
            continue;
        const auto blocks = serviceBlocksByHub.find(obj.hubId);
        if (blocks == serviceBlocksByHub.end())
            continue;
        const StaticObject* base = nullptr;
        double best = 1.0e30;
        for (const StaticObject* b : blocks->second)
        {
            const double d = glm::length(b->hubLocalOffsetMeters - obj.hubLocalOffsetMeters);
            if (d < best)
            {
                best = d;
                base = b;
            }
        }
        if (base)
            out.emplace_back(&obj, base);
    }
    std::sort(out.begin(), out.end(), [](const auto& x, const auto& y) { return x.first->hubModuleId < y.first->hubModuleId; });
    m_hexDockPairsCacheGeneration = m_staticObjectsGeneration;
    m_hexDockPairsCacheValid = true;
    return out;
}

bool GameSimulation::hexDockPair(const std::string& dockModuleId, const StaticObject*& dock, const StaticObject*& base) const
{
    dock = nullptr;
    base = nullptr;
    for (const auto& [d, b] : hexDockPairs())
        if (d->hubModuleId == dockModuleId)
        {
            dock = d;
            base = b;
            return true;
        }
    return false;
}

GameSimulation::DockSite* GameSimulation::pilotDockSite(EntityId shipId)
{
    const auto it = m_pilotDockSite.find(shipId);
    if (it == m_pilotDockSite.end())
        return nullptr;
    const auto site = m_dockSites.find(it->second);
    return site != m_dockSites.end() && site->second.traffic ? &site->second : nullptr;
}

const GameSimulation::DockSite* GameSimulation::pilotDockSite(EntityId shipId) const
{
    return const_cast<GameSimulation*>(this)->pilotDockSite(shipId);
}

void GameSimulation::updateDockServiceTraffic(double /*dt*/)
{
    if (m_dockTrafficDisabled)
        return;
    if (m_dockSites.empty() && std::getenv("ELITE_DOCK_TRAFFIC") && std::getenv("ELITE_DOCK_TRAFFIC")[0] == '0')
    {
        m_dockTrafficDisabled = true;
        return;
    }
    int index = 0;
    for (const auto& [dock, base] : hexDockPairs())
    {
        if (dock->systemId == m_activeCelestialSystemId)
            updateDockSiteTraffic(m_dockSites[dock->hubModuleId], dock, base, index);
        ++index;
    }
}

void GameSimulation::updateDockSiteTraffic(DockSite& site, const StaticObject* dock, const StaticObject* base, int siteIndex)
{

    const double now = m_serverTimelineClock.timeSeconds();
    const double omega = glm::radians(dock->hubLocalAngularVelocityDegPerSecond.z);
    const double phaseNow = glm::radians(game::navigation::wrapHubLocalDegrees(
        dock->hubLocalRotationDeg.z + dock->hubLocalAngularVelocityDegPerSecond.z * m_orbitalUniverseTimeSeconds));
    site.phaseOffsetRad = phaseNow - omega * now;

    if (!site.traffic)
    {
        site.dockModuleId = dock->hubModuleId;
        site.hubId = dock->hubId;
        game::traffic::dock::TrafficConfig config;
        // Generic traffic: identity of THIS hub's dock sector, its namespaces and the network it is
        // connected to come from the hub record and the world-model routes (GENERATED world sites copy).
        config.hubId = dock->hubId;
        config.systemId = dock->systemId;
        // e.g. earth_orbital_hub.hex_dock_a (Vysokiy keeps its ids), plane_demo.dock
        config.dockFacilityId = dock->hubModuleId.rfind("hub_", 0) == 0 ? dock->hubId + "." + dock->hubModuleId.substr(4)
                                                                         : dock->hubModuleId;
        config.dockFrameId = dock->hubModuleId;
        if (const auto hubIt = m_orbitalHubs.find(dock->hubId); hubIt != m_orbitalHubs.end())
        {
            config.hubDisplayName = hubIt->second.name;
            const std::string& parent = hubIt->second.parentBodyId;
            config.surfaceName = parent.substr(parent.rfind('.') + 1) + ": наземные космопорты";
        }
        {
            // callsign / flight namespace per hub: Vysokiy keeps VYS; others take their id's last segment
            std::string key = dock->hubId == "earth_orbital_hub" ? std::string("vys") : dock->hubId.substr(dock->hubId.rfind('.') + 1);
            std::string prefix;
            for (char ch : key)
                if (std::isalpha(static_cast<unsigned char>(ch)) && prefix.size() < 3)
                    prefix += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            config.shuttleCallsignPrefix = prefix;
            config.flightPrefix = prefix;
            config.cobrettCallsignPrefix = prefix + "C";
        }
        const auto& sites = world::celestial::WorldSitesCatalog::shared();   // loaded once per process
        {
            for (const auto& r : sites.routesInSystem(dock->systemId))
            {
                const std::string other = r.origin == dock->hubId ? r.destination : r.destination == dock->hubId ? r.origin : std::string();
                if (other.empty())
                    continue;
                const auto* site = sites.findSite(dock->systemId, other);
                config.connectedDestinations.push_back(site ? site->name : other);
            }
            // one visible dock sector carries its share of the hub's modelled arrivals
            // the visible HexDock is one 6-pad MODULE of the hub: it carries its share of the modelled arrivals
            if (const auto* self = sites.findSite(dock->systemId, dock->hubId); self && self->arrivalsPerDay > 0)
            {
                const int modules = self->dockModules > 0 ? self->dockModules : std::max(1, self->dockSectors);
                config.shuttleIntervalSeconds = std::clamp(86400.0 / (static_cast<double>(self->arrivalsPerDay) / modules), 300.0, 3600.0);
            }
        }
        config.cobrettCount = envInt("ELITE_DOCK_TRAFFIC_COBRETTS", config.cobrettCount);
        // ELITE_DOCK_TRAFFIC_EMERGENCY=1: evacuation / combat - no flow control (queues may grow)
        config.emergency = envInt("ELITE_DOCK_TRAFFIC_EMERGENCY", 0) != 0;
        config.shuttleIntervalSeconds = envDouble("ELITE_DOCK_TRAFFIC_INTERVAL", config.shuttleIntervalSeconds);
        // Earth = hub radial down. The FIXED frame is hub visual basis x
        // Rx Ry (yaw 27 deg about the hub radial Y), so Earth is exactly -Y.
        config.earthDirectionFixed = glm::dvec3(0.0, -1.0, 0.0);
        // every site its own range of traffic ship ids (instance ids stay unique across docks)
        site.traffic = std::make_unique<game::traffic::dock::DockServiceTraffic>(
            config, [s = &site, omega](double t) { return s->phaseOffsetRad + omega * t; },
            9000001ull + static_cast<std::uint64_t>(siteIndex) * 1000000ull);
        if (const char* path = std::getenv("ELITE_DOCK_TRAFFIC_LOG"); path && !m_dockTrafficLog)
            m_dockTrafficLog = std::fopen(path, "w");
        site.zoneRadiusMeters = site.traffic->controlledZoneRadius();
        std::cerr << "[DockTraffic] started at " << config.hubDisplayName << " (" << config.dockFacilityId << ", namespace "
                  << config.flightPrefix << ", " << config.connectedDestinations.size() << " connected destinations): cobretts=" << config.cobrettCount
                  << " shuttle_interval_s=" << config.shuttleIntervalSeconds
                  << " hub_zone_m=" << site.zoneRadiusMeters << "\n";
    }

    site.traffic->advance(now);
    for (const auto& line : site.traffic->drainLog())
    {
        if (m_dockTrafficLog)
        {
            std::fprintf(m_dockTrafficLog, "%9.1f %s\n", line.timeSeconds, line.text.c_str());
            std::fflush(m_dockTrafficLog);
        }
        else if (line.text.rfind("[dispatcher]", 0) == 0)
            std::cerr << "[DockTraffic] " << line.text << "\n";
    }

    const glm::dvec3 origin = world::coordinates::fullMeters(dock->worldPosition);
    const glm::dmat3 fixedBasis(base->orientation);
    const glm::dvec3 dockVelocity(dock->linearVelocity);

    // DEV: ELITE_DEV_DOCK_OBSERVER=1 parks the player ships 2.7 km in front of
    // the dock entry, at rest relative to it and facing the hold "flower";
    // =2 also keeps that view every tick (the hub frame turns with its orbit,
    // an inertial attitude would drift off the dock) - scripted captures.
    const char* observerEnv = std::getenv("ELITE_DEV_DOCK_OBSERVER");
    // =3: the held view from ~1.1 km in front of the hold "flower" (holds within +-22 deg).
    // =4: held at the =2 point but looking 45 deg down toward the planet limb
    // (planet terrain / wireframe cost captures).
    const bool observerPlanet = observerEnv && observerEnv[0] == '4';
    const bool observerHold = observerEnv && (observerEnv[0] == '2' || observerEnv[0] == '3' || observerPlanet);
    const bool observerNear = observerEnv && observerEnv[0] == '3';
    if ((!m_dockObserverPlaced || observerHold) && observerEnv && (observerEnv[0] == '1' || observerHold))
    {
        const glm::dvec3 eye = origin + fixedBasis * (observerNear ? glm::dvec3(250.0, 100.0, -2700.0)
                                                                   : glm::dvec3(700.0, 250.0, -2600.0));
        const glm::dvec3 look = origin + fixedBasis * (observerNear ? glm::dvec3(0.0, 0.0, -1500.0)
                                                                    : glm::dvec3(0.0, 0.0, -900.0));
        glm::dvec3 f = glm::normalize(look - eye);
        if (observerPlanet)
            for (auto& [id, shipPtr] : m_ships)
                if (shipPtr && isPlayerControlled(id))
                    if (const auto* frame = hubNavigationFrame(shipPtr->core().transform().motion.hubId); frame && frame->valid)
                    {
                        f = glm::normalize(-frame->radialAxis + frame->progradeAxis);
                        break;
                    }
        const glm::dvec3 r = glm::normalize(glm::cross(f, fixedBasis[1]));
        const glm::dvec3 u = glm::cross(r, f);
        glm::mat4 orientation(1.0f);
        orientation[0] = glm::vec4(glm::vec3(r), 0.0f);
        orientation[1] = glm::vec4(glm::vec3(u), 0.0f);
        orientation[2] = glm::vec4(glm::vec3(-f), 0.0f);
        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr || !isPlayerControlled(id))
                continue;
            auto& tr = shipPtr->core().transform();
            tr.orientation = orientation;
            const auto* frame = tr.motion.mode == game::navigation::MotionMode::HubTactical
                ? hubNavigationFrame(tr.motion.hubId) : nullptr;
            if (frame && frame->valid)
            {
                const glm::dvec3 local = frame->worldToLocalPosition(eye);
                tr.motion.localPositionMeters = local;
                tr.motion.localVelocityMps = frame->worldToLocalVelocity(eye, dockVelocity);
                tr.motion.referenceVelocityMps = tr.motion.travelFrame.localToWorldVelocity(local, glm::dvec3(0.0));
                tr.referenceVelocityMetersPerSecond = tr.motion.referenceVelocityMps;
            }
            tr.motion.worldVelocityMps = dockVelocity;
            tr.setWorldPositionMeters(eye);
            if (m_dockObserverPlaced)
                continue;
            m_dockObserverPlaced = true;
            std::cerr << "[DockTraffic] DEV observer: player " << id.value << " parked 2.7 km in front of the dock\n";
        }
    }

    const bool refreshRoutes = now - site.routeSeconds >= 1.0;
    if (refreshRoutes)
        site.routeSeconds = now;

    std::unordered_set<std::uint64_t> alive;
    for (const auto& actor : site.traffic->actors())
    {
        const auto pose = site.traffic->pose(actor.id, now);
        if (!pose.present)
            continue;
        alive.insert(actor.id);
        const glm::dvec3 position = origin + fixedBasis * pose.position;
        const glm::dvec3 velocity = dockVelocity + fixedBasis * pose.velocity;
        const glm::dmat3 basis = fixedBasis * pose.basis;
        glm::mat4 orientation(1.0f);
        orientation[0] = glm::vec4(glm::vec3(basis[0]), 0.0f);
        orientation[1] = glm::vec4(glm::vec3(basis[1]), 0.0f);
        orientation[2] = glm::vec4(glm::vec3(basis[2]), 0.0f);

        auto shellIt = site.shellByActor.find(actor.id);
        if (shellIt == site.shellByActor.end())
        {
            const bool shuttle = actor.kind == game::traffic::dock::TrafficShipKind::CargoShuttle;
            const ShipDescriptor& descriptor = shuttle
                ? game::ship::descriptors::orbitalCargoShuttleDescriptor()
                : EliteCobraMk1::EliteCobraMk1Descriptor();
            ShipInitData init;
            init.visual.shipType = shuttle ? "OFU OCS-4" : "Cobrett-I";
            init.visual.shipName = actor.callsign;
            init.registry.instanceId = actor.id;
            init.registry.ownerName = shuttle ? "Orbital Freight Union" : "Hub traffic";
            init.registry.registrationId = actor.callsign;
            init.registry.shipRole = ShipRoleType::Civilian;
            const EntityId id = spawnTrafficShell(dock->systemId, descriptor, position, velocity, init, orientation, 0, 0);
            if (id.value == 0)
                continue;
            shellIt = site.shellByActor.emplace(actor.id, id).first;
            m_dockTrafficShells.insert(id);
        }
        // The motion-fidelity LOD materializes a shell near a player (same
        // EntityId / ShipInstanceId). The traffic model still owns its pose:
        // a materialized traffic ship is kinematic and skips the physics and
        // AI loops (isDockStationControlledShip).
        ShipTransform* target = nullptr;
        if (auto it = m_shipShells.find(shellIt->second); it != m_shipShells.end())
            target = &it->second.transform;
        else if (Ship* ship = getShip(shellIt->second))
            target = &ship->core().transform();
        if (!target)
            continue;
        auto& tr = *target;
        // collision avoidance (docs/HUB_SECURITY.md): a flying traffic ship evades a real ship on a collision course within
        // its own limits - its scripted pose + the Newtonian offset layer; not on / near the pads (the dispatcher's area)
        glm::dvec3 flownPosition = position;
        glm::dvec3 flownVelocity = velocity;
        if ((glm::length(pose.velocity) > 3.0 && glm::length(pose.position) > 300.0) || m_evasion.count(shellIt->second) != 0)
        {
            const bool shuttleHull = actor.kind == game::traffic::dock::TrafficShipKind::CargoShuttle;
            const glm::dvec3 forward = glm::length(pose.velocity) > 1.0e-3 ? glm::normalize(fixedBasis * pose.velocity) : -basis[2];
            applyTrafficEvasion(shellIt->second, flownPosition, flownVelocity, forward, basis[1], shuttleHull ? 56.0 : 25.0, 0.0,
                                dock->hubId);
        }
        tr.setWorldPositionMeters(flownPosition);
        tr.orientation = orientation;
        tr.motion.worldVelocityMps = flownVelocity;
        tr.motion.localVelocityMps = flownVelocity;
        // Map ownership: inside the dock's controlled zone the ship belongs to
        // this hub (Hub Map); outside it, to the wider maps.
        tr.motion.hubId =
            glm::length(pose.position) <= site.zoneRadiusMeters ? dock->hubId : std::string {};

        if (refreshRoutes)
        {
            auto& info = m_dockTrafficInfo[shellIt->second];
            info.active = true;
            info.callsign = actor.callsign;
            info.phase = game::traffic::dock::actorPhaseName(actor.phase);
            info.destination = site.traffic->destination(actor.id);
            info.routeOffsetsMeters.clear();
            for (const auto& p : site.traffic->plannedPath(actor.id, now))
                info.routeOffsetsMeters.push_back(fixedBasis * (p - pose.position));
        }
    }
    // Live status every 10 s of server time: where each traffic ship is, how
    // the server simulates it and how far the nearest player is.
    if (m_dockTrafficLog && now - site.statusSeconds >= 10.0)
    {
        site.statusSeconds = now;
        const glm::dmat3 toFixed = glm::transpose(fixedBasis);
        std::vector<glm::dvec3> players;
        for (const auto& [id, shipPtr] : m_ships)
            if (shipPtr && isPlayerControlled(id))
                players.push_back(shipPtr->core().transform().fullWorldMeters());
        for (const auto& actor : site.traffic->actors())
        {
            const auto pose = site.traffic->pose(actor.id, now);
            const auto shellIt = site.shellByActor.find(actor.id);
            const char* tier = "none";
            int fidelity = -1;
            glm::dvec3 world = origin + fixedBasis * pose.position;
            if (shellIt != site.shellByActor.end())
            {
                if (const auto it = m_shipShells.find(shellIt->second); it != m_shipShells.end())
                {
                    tier = "shell";
                    fidelity = static_cast<int>(it->second.lifecycle.motionFidelity);
                    world = it->second.transform.fullWorldMeters();
                }
                else if (const Ship* ship = getShip(shellIt->second))
                {
                    tier = "MATERIALIZED";
                    world = ship->core().transform().fullWorldMeters();
                }
            }
            double nearest = -1.0;
            for (const auto& p : players)
                nearest = nearest < 0.0 ? glm::length(p - world) : std::min(nearest, glm::length(p - world));
            const glm::dvec3 f = toFixed * (world - origin);
            std::fprintf(m_dockTrafficLog,
                         "%9.1f [status] %s phase=%s present=%d tier=%s fidelity=%d entity=%llu "
                         "fixed=(%.0f,%.0f,%.0f) dock_m=%.0f player_m=%.0f\n",
                         now, actor.callsign.c_str(), game::traffic::dock::actorPhaseName(actor.phase),
                         pose.present ? 1 : 0, tier, fidelity,
                         static_cast<unsigned long long>(
                             shellIt != site.shellByActor.end() ? shellIt->second.value : 0),
                         f.x, f.y, f.z, glm::length(world - origin), nearest);
        }
        for (const auto& p : players)
        {
            const glm::dvec3 f = toFixed * (p - origin);
            std::fprintf(m_dockTrafficLog, "%9.1f [status] player fixed=(%.0f,%.0f,%.0f) dock_m=%.0f\n", now, f.x,
                         f.y, f.z, glm::length(p - origin));
        }
        std::fflush(m_dockTrafficLog);
    }

    for (auto it = site.shellByActor.begin(); it != site.shellByActor.end();)
    {
        if (alive.count(it->first) == 0)
        {
            m_dockTrafficShells.erase(it->second);
            m_dockTrafficInfo.erase(it->second);
            if (getShip(it->second))
                dematerializeShip(it->second, game::simulation::ShipSimulationTier::TrafficShell);
            removeTrafficShell(it->second);
            it = site.shellByActor.erase(it);
        }
        else
            ++it;
    }
}

bool GameSimulation::isDockStationControlledShip(EntityId id) const noexcept
{
    return m_dockStationControlledShips.count(id) != 0 || m_dockTrafficShells.count(id) != 0 || m_airTrafficShells.count(id) != 0;
}

game::docking::traffic::DockTrafficShipStatus GameSimulation::dockDispatcherStatus(EntityId shipId) const
{
    const DockSite* site = pilotDockSite(shipId);
    if (!site)
        return {};
    return site->traffic->dispatcher().status(shipId.value);
}

bool GameSimulation::requestDockArrival(EntityId shipId, bool engage, const std::string& dockModuleId)
{
    (void)commandContextOk(shipId, "requestDockArrival");
    using game::docking::traffic::DockTrafficPhase;
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    const double now = m_serverTimelineClock.timeSeconds();
    if (!engage)
    {
        DockSite* site = pilotDockSite(shipId);
        if (!site)
            return true;
        auto& dispatcher = site->traffic->dispatcher();
        const auto status = dispatcher.status(shipId.value);
        if (status.phase == DockTrafficPhase::Queued || status.phase == DockTrafficPhase::HoldAssigned ||
            status.phase == DockTrafficPhase::AtHold || status.phase == DockTrafficPhase::EntryCleared)
        {
            dispatcher.cancel(shipId.value, now);
            m_pilotDockSite.erase(shipId);
            std::cerr << "[DockTraffic] pilot " << shipId.value << " docking at " << site->dockModuleId
                      << " dropped by the pilot, request cancelled\n";
        }
        return true;
    }
    // the dock asked for; without one, the nearest dock site of the ship's system
    const StaticObject* dock = nullptr;
    const StaticObject* base = nullptr;
    if (!dockModuleId.empty())
        hexDockPair(dockModuleId, dock, base);
    else
    {
        double best = 1.0e30;
        for (const auto& [d, b] : hexDockPairs())
        {
            const double dist = glm::length(world::coordinates::fullMeters(d->worldPosition) -
                                            ship->core().transform().fullWorldMeters());
            if (d->systemId == ship->core().transform().motion.systemId && dist < best)
            {
                best = dist;
                dock = d;
                base = b;
            }
        }
    }
    if (!dock || !base)
    {
        std::cerr << "[DockTraffic] pilot " << shipId.value << " docking request: " << dockModuleId
                  << " is not a hex dock - no dispatcher\n";
        return false;
    }
    auto siteIt = m_dockSites.find(dock->hubModuleId);
    if (siteIt == m_dockSites.end() || !siteIt->second.traffic)
    {
        std::cerr << "[DockTraffic] pilot " << shipId.value << " docking request: the dispatcher of " << dock->hubModuleId
                  << " is not running (another system?)\n";
        return false;
    }
    DockSite& site = siteIt->second;
    // asked another dock while assigned somewhere: leave that queue first
    if (DockSite* previous = pilotDockSite(shipId); previous && previous != &site)
    {
        previous->traffic->dispatcher().cancel(shipId.value, now);
        m_pilotDockSite.erase(shipId);
    }
    auto& dispatcher = site.traffic->dispatcher();
    const auto status = dispatcher.status(shipId.value);
    if (status.phase != DockTrafficPhase::None && status.phase != DockTrafficPhase::Departed)
        return true;   // already known (queued / holding / cleared / docked)
    if (status.phase == DockTrafficPhase::Departed)
        dispatcher.cancel(shipId.value, now);
    const glm::dvec3 origin = world::coordinates::fullMeters(dock->worldPosition);
    const glm::dmat3 toFixed = glm::transpose(glm::dmat3(base->orientation));
    const glm::dvec3 p = toFixed * (ship->core().transform().fullWorldMeters() - origin);
    game::docking::traffic::DockingTrafficRequest r;
    r.shipId = shipId.value;
    r.shipClass = game::docking::DockSizeClass::Medium;
    r.envelope = game::traffic::dock::dockingEnvelope(
        game::traffic::dock::trafficShipClass(game::traffic::dock::TrafficShipKind::Cobrett));
    r.insideControlledApproachHorizon = true;
    // The pilot's planner / autopilot fly their own route to the dock: a direct-entry request - first in the queue;
    // the station captures it on the approach axis once its entry is cleared.
    const bool direct = true;
    const bool freeNow = dispatcher.entryFreeNow(r);
    constexpr double TwoPi = 2.0 * 3.14159265358979323846;
    const double azimuth = std::fmod(std::atan2(p.y, p.x) + TwoPi, TwoPi);
    const bool ok = dispatcher.requestDocking(r, now, azimuth, direct).committed;
    if (ok)
        m_pilotDockSite[shipId] = site.dockModuleId;
    std::cerr << "[DockTraffic] pilot " << shipId.value << " automatic docking request at " << site.dockModuleId << " "
              << (ok ? "accepted" : "rejected") << (freeNow ? " - the entrance is free" : " - the entrance is busy, queued first")
              << " (fixed z=" << p.z << ", " << glm::length(p) << " m from the dock)\n";
    return ok;
}

void GameSimulation::fillDockDispatcherGeometry(EntityId shipId, game::simulation::ShipDockDispatcherSnapshot& out) const
{
    namespace hex = game::docking::hex;
    const DockSite* site = pilotDockSite(shipId);
    const StaticObject* dock = nullptr;
    const StaticObject* base = nullptr;
    if (site)
        hexDockPair(site->dockModuleId, dock, base);
    const Ship* ship = nullptr;
    if (const auto it = m_ships.find(shipId); it != m_ships.end())
        ship = it->second.get();
    if (!dock || !base || !ship)
        return;
    const auto& tr = ship->core().transform();
    const auto* frame = tr.motion.mode == game::navigation::MotionMode::HubTactical ? hubNavigationFrame(tr.motion.hubId) : nullptr;
    if (!frame || !frame->valid)
        return;
    const auto& g = hex::hexDockGeometry();
    const glm::dvec3 origin = world::coordinates::fullMeters(dock->worldPosition);
    const glm::dmat3 fixedBasis(base->orientation);
    const glm::dvec3 entryWorld = origin + fixedBasis * glm::dvec3(0.0, 0.0, g.entryFaceZ);
    out.entryLocalMeters = frame->worldToLocalPosition(entryWorld);
    const glm::dvec3 outLocal = frame->worldToLocalPosition(entryWorld + fixedBasis * glm::dvec3(0.0, 0.0, -1000.0)) - out.entryLocalMeters;
    out.axisOutLocal = glm::normalize(outLocal);
    const int k = hex::indexOfId(out.holdPointId);
    if (out.holdPointId.size() == 2 && out.holdPointId[0] == 'H' && k >= 0)
    {
        out.holdValid = true;
        out.holdLocalMeters = frame->worldToLocalPosition(origin + fixedBasis * hex::holdPosition(g, k));
    }
}

bool GameSimulation::requestDockDeparture(EntityId shipId)
{
    (void)commandContextOk(shipId, "requestDockDeparture");
    DockSite* site = pilotDockSite(shipId);
    if (!site)
        return false;
    const bool ok = site->traffic->requestExternalDeparture(shipId.value, m_serverTimelineClock.timeSeconds());
    std::cerr << "[DockTraffic] pilot " << shipId.value << " departure request at " << site->dockModuleId << " "
              << (ok ? "accepted" : "rejected") << "\n";
    return ok;
}

bool GameSimulation::releaseDockDepartureToPilot(EntityId shipId)
{
    DockSite* site = pilotDockSite(shipId);
    if (!site)
        return false;
    const bool ok = site->traffic->releaseExternalToPilot(shipId.value, m_serverTimelineClock.timeSeconds());
    std::cerr << "[DockTraffic] pilot " << shipId.value << " takes the controls during the departure: "
              << (ok ? "released early" : "refused (still inside the dock)") << "\n";
    return ok;
}

// Port fees (docs/TRADE_DESIGN.md §5.2, step 4). Called for every pilot ship of a dock site in the ship's own system
// pass. The dispatcher's Docked phase opens a call (the docking fee, once: 150 + 0.9 Cr/t dry mass x the port index,
// 4 h at the berth included); leaving Docked closes it (the berth fee beyond the included hours). The time at the berth
// is the server timeline clock; DEV: ELITE_DEV_BERTH_TIME_SCALE=<k> counts k berth seconds per server second (a live
// check of the parking fee without hours of waiting). The economy books both idempotently per callId and never refuses.
void GameSimulation::updatePortCall(EntityId id, const DockSite& site, bool docked)
{
    const double now = m_serverTimelineClock.timeSeconds();
    const auto it = m_portCalls.find(id);
    if (docked)
    {
        if (it != m_portCalls.end())
            return;   // the same call: Docked repeats every tick
        const Ship* ship = getShip(id);
        if (!ship)
            return;
        PortCall call;
        call.hubId = site.hubId;
        call.dockModuleId = site.dockModuleId;
        call.dockedAtSeconds = now;
        call.callId = "call:" + std::to_string(id.value) + ":" + site.dockModuleId + ":" +
                      std::to_string(static_cast<long long>(std::llround(now * 1000.0)));
        // the hull's dry mass: the catalog passport; a legacy hull: its mass without the cargo and the C20 tare
        const auto& core = ship->core();
        double dryT = core.descriptor().catalog.dryMassTonnes;
        if (!(dryT > 0.0))
            dryT = std::max(0.0, core.currentMassKg() / 1000.0 - core.load().cargoTonnes - 1.7 * std::max(0, core.load().containers));
        const auto fee = game::economy::service::chargeDockingFee(static_cast<std::uint32_t>(id.value), site.hubId, call.callId, dryT);
        std::cerr << "[PortFee] ship " << id.value << " docked at " << site.hubId << " (" << site.dockModuleId << "), call " << call.callId
                  << ": docking fee " << fee.feeCr << " Cr (" << (fee.charged ? "booked" : fee.duplicate ? "duplicate" : "no economy / AI")
                  << ", paid " << fee.paidCr << ", debt " << fee.debtCr << ", dry " << dryT << " t)" << std::endl;
        m_portCalls.emplace(id, std::move(call));
        return;
    }
    if (it == m_portCalls.end())
        return;
    const double scale = std::max(0.0, envDouble("ELITE_DEV_BERTH_TIME_SCALE", 1.0));
    const double hours = std::max(0.0, now - it->second.dockedAtSeconds) * scale / 3600.0;
    const auto fee = game::economy::service::chargeBerthFee(static_cast<std::uint32_t>(id.value), it->second.hubId, it->second.callId, hours);
    std::cerr << "[PortFee] ship " << id.value << " left the berth at " << it->second.hubId << ", call " << it->second.callId << ": "
              << hours << " h at the berth" << (scale != 1.0 ? " (DEV berth time x" + std::to_string(scale) + ")" : std::string())
              << " -> berth fee " << fee.feeCr << " Cr (" << (fee.charged ? "booked" : fee.duplicate ? "duplicate" : "nothing booked")
              << ", paid " << fee.paidCr << ", debt " << fee.debtCr << ")" << std::endl;
    m_portCalls.erase(it);
}

void GameSimulation::updateDockPilotShips()
{
    for (const auto& [dock, base] : hexDockPairs())
    {
        const auto it = m_dockSites.find(dock->hubModuleId);
        if (it != m_dockSites.end() && it->second.traffic && dock->systemId == m_activeCelestialSystemId)
            updateDockSitePilots(it->second, dock, base);
    }
}

void GameSimulation::updateDockSitePilots(DockSite& site, const StaticObject* dock, const StaticObject* base)
{
    using game::docking::traffic::DockTrafficPhase;

    const double now = m_serverTimelineClock.timeSeconds();
    const glm::dvec3 origin = world::coordinates::fullMeters(dock->worldPosition);
    const glm::dmat3 fixedBasis(base->orientation);
    const glm::dmat3 toFixed = glm::transpose(fixedBasis);
    const glm::dvec3 dockVelocity(dock->linearVelocity);
    auto& traffic = *site.traffic;
    auto& dispatcher = traffic.dispatcher();
    const auto envelope = game::traffic::dock::dockingEnvelope(
        game::traffic::dock::trafficShipClass(game::traffic::dock::TrafficShipKind::Cobrett));

    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr || !isPlayerControlled(id) ||
            !game::simulation::sameRuntimeSystem(shipPtr->core().transform().motion.systemId, dock->systemId))
            continue;
        // a pilot belongs to one site: skip the ships another dock is handling
        if (const auto own = m_pilotDockSite.find(id); own != m_pilotDockSite.end() && own->second != site.dockModuleId)
            continue;
        auto& tr = shipPtr->core().transform();
        const glm::dvec3 p = toFixed * (tr.fullWorldMeters() - origin);
        const glm::dvec3 v = toFixed * (tr.motion.worldVelocityMps - dockVelocity);
        const auto status = dispatcher.status(id.value);
        updatePortCall(id, site, status.phase == DockTrafficPhase::Docked);
        const double radial = std::hypot(p.x, p.y);
        if (status.phase != DockTrafficPhase::None && now - m_dockPilotTraceSeconds >= 5.0)
        {
            m_dockPilotTraceSeconds = now;
            std::cerr << "[DockTraffic] pilot " << id.value << " trace phase="
                      << game::docking::traffic::dockTrafficPhaseName(status.phase) << " fixed=(" << p.x << ","
                      << p.y << "," << p.z << ") speed=" << glm::length(v)
                      << " station=" << isDockStationControlledShip(id) << "\n";
        }

        if (!isDockStationControlledShip(id))
        {
            switch (status.phase)
            {
                case DockTrafficPhase::None:
                case DockTrafficPhase::Departed:
                    // Approach zone on the axis in front of the slot: the ship
                    // calls the dispatcher (later: an explicit comm request).
                    if (p.z > -1500.0 && p.z < -150.0 && radial < 260.0 && glm::length(v) < 40.0)
                    {
                        if (status.phase == DockTrafficPhase::Departed)
                            dispatcher.cancel(id.value, now);
                        game::docking::traffic::DockingTrafficRequest r;
                        r.shipId = id.value;
                        r.shipClass = game::docking::DockSizeClass::Medium;
                        r.envelope = envelope;
                        r.insideControlledApproachHorizon = true;
                        const bool ok = dispatcher.requestDocking(r, now, -1.0, true).committed;
                        if (ok)
                            m_pilotDockSite[id] = site.dockModuleId;
                        std::cerr << "[DockTraffic] pilot " << id.value << " docking request at " << site.dockModuleId << " "
                                  << (ok ? "accepted" : "rejected") << " at z=" << p.z << "\n";
                    }
                    break;
                case DockTrafficPhase::HoldAssigned:
                {
                    const int k = game::docking::hex::indexOfId(status.holdPointId);
                    if (glm::length(p) > RequestDropMeters)
                    {
                        dispatcher.cancel(id.value, now);
                        m_pilotDockSite.erase(id);
                        std::cerr << "[DockTraffic] pilot " << id.value << " left the zone, request cancelled\n";
                    }
                    else if (k >= 0 &&
                             glm::length(p - game::docking::hex::holdPosition(game::docking::hex::hexDockGeometry(), k)) < HoldReachedMeters &&
                             glm::length(v) < HoldReachedMps)
                    {
                        dispatcher.reportAtHold(id.value, now);
                        std::cerr << "[DockTraffic] pilot " << id.value << " at hold " << status.holdPointId << "\n";
                    }
                    break;
                }
                case DockTrafficPhase::AtHold:
                    if (glm::length(p) > RequestDropMeters)
                    {
                        dispatcher.cancel(id.value, now);
                        m_pilotDockSite.erase(id);
                        std::cerr << "[DockTraffic] pilot " << id.value << " left the zone, request cancelled\n";
                    }
                    break;
                case DockTrafficPhase::Queued:
                case DockTrafficPhase::EntryCleared:
                    if (glm::length(p) > RequestDropMeters)
                    {
                        dispatcher.cancel(id.value, now);
                        m_pilotDockSite.erase(id);
                        std::cerr << "[DockTraffic] pilot " << id.value << " left the zone, request cancelled\n";
                    }
                    else if (status.phase == DockTrafficPhase::EntryCleared &&
                             radial < 60.0 && p.z > -1000.0 && p.z < -150.0 && glm::length(v) < 25.0)
                    {
                        game::traffic::dock::TrafficPose pose;
                        pose.present = true;
                        pose.position = p;
                        pose.basis = toFixed * glm::dmat3(glm::mat3(tr.orientation));
                        pose.velocity = v;
                        if (traffic.beginStationCapture(id.value, game::traffic::dock::TrafficShipKind::Cobrett, pose, now))
                        {
                            m_dockStationControlledShips.insert(id);
                            std::cerr << "[DockTraffic] pilot " << id.value << " captured by station control at "
                                      << site.dockModuleId << " -> " << status.padId << "\n";
                        }
                    }
                    break;
                case DockTrafficPhase::DepartureCleared:
                    if (p.z <= -2400.0 || glm::length(p) > 3000.0 || radial > 300.0)
                    {
                        dispatcher.reportClear(id.value, now);
                        m_pilotDockSite.erase(id);
                        std::cerr << "[DockTraffic] pilot " << id.value << " clear of the dock lane\n";
                    }
                    break;
                default:
                    break;
            }
            continue;
        }

        // DEV smoke test: ELITE_DEV_DOCK_AUTO_DEPART=<s> asks for departure
        // after that long on the pad (the pilot's L key otherwise).
        if (status.phase == DockTrafficPhase::Docked && now - status.phaseSinceSeconds > envDouble("ELITE_DEV_DOCK_AUTO_DEPART", 1e30))
            requestDockDeparture(id);

        const auto pose = traffic.pose(id.value, now);
        if (!pose.present)
        {
            // Released by the station (at rest beyond the dock lanes): physics again.
            m_dockStationControlledShips.erase(id);
            std::cerr << "[DockTraffic] pilot " << id.value << " released by the station: free flight\n";
            continue;
        }
        const glm::dvec3 position = origin + fixedBasis * pose.position;
        const glm::dvec3 velocity = dockVelocity + fixedBasis * pose.velocity;
        const glm::dmat3 basis = fixedBasis * pose.basis;
        glm::mat4 orientation(1.0f);
        orientation[0] = glm::vec4(glm::vec3(basis[0]), 0.0f);
        orientation[1] = glm::vec4(glm::vec3(basis[1]), 0.0f);
        orientation[2] = glm::vec4(glm::vec3(basis[2]), 0.0f);
        tr.orientation = orientation;
        const auto* frame = tr.motion.mode == game::navigation::MotionMode::HubTactical
            ? hubNavigationFrame(tr.motion.hubId) : nullptr;
        if (frame && frame->valid)
        {
            // The per-tick matched-frame refresh skips station-held ships: keep the ship's travel frame on the moving
            // hub here, or its local offset (written against the live hub) lands on a stale origin at the release
            // (live 2026-11-18: normal flight right after the release put the ship 96 km under Wolf 359 e - crash).
            game::navigation::TravelFrameSystem::refreshMatchedReference(tr.motion, frame->kinematicFrame(), frame->hubId);
            const glm::dvec3 local = frame->worldToLocalPosition(position);
            tr.motion.localPositionMeters = local;
            tr.motion.localVelocityMps = frame->worldToLocalVelocity(position, velocity);
            tr.motion.referenceVelocityMps = tr.motion.travelFrame.localToWorldVelocity(local, glm::dvec3(0.0));
            tr.referenceVelocityMetersPerSecond = tr.motion.referenceVelocityMps;
        }
        tr.motion.worldVelocityMps = velocity;
        tr.setWorldPositionMeters(position);
    }
}
