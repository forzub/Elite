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

namespace
{
constexpr const char* DockModuleId = "hub_hex_dock_a";
constexpr const char* DockBaseModuleId = "hub_hex_dock_base";

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
} // namespace

game::traffic::dock::DockServiceTraffic* GameSimulation::dockServiceTraffic() noexcept
{
    return m_dockServiceTraffic.get();
}

bool GameSimulation::removeTrafficShell(EntityId id)
{
    const auto it = m_shipShells.find(id);
    if (it == m_shipShells.end())
        return false;
    m_shipShells.erase(it);
    m_shipMotionFidelityStates.erase(id);
    m_shipClientObservationInterests.erase(id);
    return true;
}

void GameSimulation::updateDockServiceTraffic(double /*dt*/)
{
    if (m_dockTrafficDisabled)
        return;
    if (!m_dockServiceTraffic && std::getenv("ELITE_DOCK_TRAFFIC") && std::getenv("ELITE_DOCK_TRAFFIC")[0] == '0')
    {
        m_dockTrafficDisabled = true;
        return;
    }

    const StaticObject* dock = nullptr;
    const StaticObject* base = nullptr;
    for (const auto& [id, obj] : m_staticObjects)
    {
        if (!obj.attachedToHub)
            continue;
        if (obj.hubModuleId == DockModuleId)
            dock = &obj;
        else if (obj.hubModuleId == DockBaseModuleId)
            base = &obj;
    }
    if (!dock || !base || dock->systemId != m_activeCelestialSystemId)
        return;

    const double now = m_serverTimelineClock.timeSeconds();
    const double omega = glm::radians(dock->hubLocalAngularVelocityDegPerSecond.z);
    const double phaseNow = glm::radians(game::navigation::wrapHubLocalDegrees(
        dock->hubLocalRotationDeg.z + dock->hubLocalAngularVelocityDegPerSecond.z * m_orbitalUniverseTimeSeconds));
    m_dockTrafficPhaseOffsetRad = phaseNow - omega * now;

    if (!m_dockServiceTraffic)
    {
        game::traffic::dock::TrafficConfig config;
        // Generic traffic: identity of THIS hub's dock sector, its namespaces and the network it is
        // connected to come from the hub record and the world-model routes (GENERATED world sites copy).
        config.hubId = dock->hubId;
        config.systemId = dock->systemId;
        config.dockFacilityId = dock->hubId + "." + std::string(DockModuleId).substr(4);   // e.g. earth_orbital_hub.hex_dock_a
        config.dockFrameId = DockModuleId;
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
        world::celestial::WorldSitesCatalog sites;
        if (sites.loadFromRuntimeOrSource())
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
        config.shuttleIntervalSeconds = envDouble("ELITE_DOCK_TRAFFIC_INTERVAL", config.shuttleIntervalSeconds);
        // Earth = hub radial down. The FIXED frame is hub visual basis x
        // Rx Ry (yaw 27 deg about the hub radial Y), so Earth is exactly -Y.
        config.earthDirectionFixed = glm::dvec3(0.0, -1.0, 0.0);
        m_dockServiceTraffic = std::make_unique<game::traffic::dock::DockServiceTraffic>(
            config, [this, omega](double t) { return m_dockTrafficPhaseOffsetRad + omega * t; });
        if (const char* path = std::getenv("ELITE_DOCK_TRAFFIC_LOG"))
            m_dockTrafficLog = std::fopen(path, "w");
        m_dockTrafficZoneRadiusMeters = m_dockServiceTraffic->controlledZoneRadius();
        std::cerr << "[DockTraffic] started at " << config.hubDisplayName << " (" << config.dockFacilityId << ", namespace "
                  << config.flightPrefix << ", " << config.connectedDestinations.size() << " connected destinations): cobretts=" << config.cobrettCount
                  << " shuttle_interval_s=" << config.shuttleIntervalSeconds
                  << " hub_zone_m=" << m_dockTrafficZoneRadiusMeters << "\n";
    }

    m_dockServiceTraffic->advance(now);
    for (const auto& line : m_dockServiceTraffic->drainLog())
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
    const bool observerHold = observerEnv && (observerEnv[0] == '2' || observerEnv[0] == '3');
    const bool observerNear = observerEnv && observerEnv[0] == '3';
    if ((!m_dockObserverPlaced || observerHold) && observerEnv && (observerEnv[0] == '1' || observerHold))
    {
        const glm::dvec3 eye = origin + fixedBasis * (observerNear ? glm::dvec3(250.0, 100.0, -2700.0)
                                                                   : glm::dvec3(700.0, 250.0, -2600.0));
        const glm::dvec3 look = origin + fixedBasis * (observerNear ? glm::dvec3(0.0, 0.0, -1500.0)
                                                                    : glm::dvec3(0.0, 0.0, -900.0));
        const glm::dvec3 f = glm::normalize(look - eye);
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

    const bool refreshRoutes = now - m_dockTrafficRouteSeconds >= 1.0;
    if (refreshRoutes)
        m_dockTrafficRouteSeconds = now;

    std::unordered_set<std::uint64_t> alive;
    for (const auto& actor : m_dockServiceTraffic->actors())
    {
        const auto pose = m_dockServiceTraffic->pose(actor.id, now);
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

        auto shellIt = m_dockTrafficShellByActor.find(actor.id);
        if (shellIt == m_dockTrafficShellByActor.end())
        {
            const bool shuttle = actor.kind == game::traffic::dock::TrafficShipKind::CargoShuttle;
            const ShipDescriptor& descriptor = shuttle
                ? game::ship::descriptors::orbitalCargoShuttleDescriptor()
                : EliteCobraMk1::EliteCobraMk1Descriptor();
            ShipInitData init;
            init.visual.shipType = shuttle ? "OCS-4 Lastochka" : "Cobrett-I";
            init.visual.shipName = actor.callsign;
            init.registry.instanceId = actor.id;
            init.registry.ownerName = shuttle ? "Orbital Freight Union" : "Hub traffic";
            init.registry.registrationId = actor.callsign;
            init.registry.shipRole = ShipRoleType::Civilian;
            const EntityId id = spawnTrafficShell(dock->systemId, descriptor, position, velocity, init, orientation, 0, 0);
            if (id.value == 0)
                continue;
            shellIt = m_dockTrafficShellByActor.emplace(actor.id, id).first;
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
        tr.setWorldPositionMeters(position);
        tr.orientation = orientation;
        tr.motion.worldVelocityMps = velocity;
        tr.motion.localVelocityMps = velocity;
        // Map ownership: inside the dock's controlled zone the ship belongs to
        // this hub (Hub Map); outside it, to the wider maps.
        tr.motion.hubId =
            glm::length(pose.position) <= m_dockTrafficZoneRadiusMeters ? dock->hubId : std::string {};

        if (refreshRoutes)
        {
            auto& info = m_dockTrafficInfo[shellIt->second];
            info.active = true;
            info.callsign = actor.callsign;
            info.phase = game::traffic::dock::actorPhaseName(actor.phase);
            info.destination = m_dockServiceTraffic->destination(actor.id);
            info.routeOffsetsMeters.clear();
            for (const auto& p : m_dockServiceTraffic->plannedPath(actor.id, now))
                info.routeOffsetsMeters.push_back(fixedBasis * (p - pose.position));
        }
    }
    // Live status every 10 s of server time: where each traffic ship is, how
    // the server simulates it and how far the nearest player is.
    if (m_dockTrafficLog && now - m_dockTrafficStatusSeconds >= 10.0)
    {
        m_dockTrafficStatusSeconds = now;
        const glm::dmat3 toFixed = glm::transpose(fixedBasis);
        std::vector<glm::dvec3> players;
        for (const auto& [id, shipPtr] : m_ships)
            if (shipPtr && isPlayerControlled(id))
                players.push_back(shipPtr->core().transform().fullWorldMeters());
        for (const auto& actor : m_dockServiceTraffic->actors())
        {
            const auto pose = m_dockServiceTraffic->pose(actor.id, now);
            const auto shellIt = m_dockTrafficShellByActor.find(actor.id);
            const char* tier = "none";
            int fidelity = -1;
            glm::dvec3 world = origin + fixedBasis * pose.position;
            if (shellIt != m_dockTrafficShellByActor.end())
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
                             shellIt != m_dockTrafficShellByActor.end() ? shellIt->second.value : 0),
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

    for (auto it = m_dockTrafficShellByActor.begin(); it != m_dockTrafficShellByActor.end();)
    {
        if (alive.count(it->first) == 0)
        {
            m_dockTrafficShells.erase(it->second);
            m_dockTrafficInfo.erase(it->second);
            if (getShip(it->second))
                dematerializeShip(it->second, game::simulation::ShipSimulationTier::TrafficShell);
            removeTrafficShell(it->second);
            it = m_dockTrafficShellByActor.erase(it);
        }
        else
            ++it;
    }
}

bool GameSimulation::isDockStationControlledShip(EntityId id) const noexcept
{
    return m_dockStationControlledShips.count(id) != 0 || m_dockTrafficShells.count(id) != 0;
}

game::docking::traffic::DockTrafficShipStatus GameSimulation::dockDispatcherStatus(EntityId shipId) const
{
    if (!m_dockServiceTraffic)
        return {};
    return m_dockServiceTraffic->dispatcher().status(shipId.value);
}

bool GameSimulation::requestDockDeparture(EntityId shipId)
{
    if (!m_dockServiceTraffic)
        return false;
    const bool ok = m_dockServiceTraffic->requestExternalDeparture(shipId.value, m_serverTimelineClock.timeSeconds());
    std::cerr << "[DockTraffic] pilot " << shipId.value << " departure request " << (ok ? "accepted" : "rejected") << "\n";
    return ok;
}

// Pilot ships (player-controlled) and the dock dispatcher. Docking is
// requested when the ship enters the approach zone on the dock axis; the
// station captures it at the portal once entry is cleared and owns its pose
// (kinematic, excluded from the physics loops) until it is released at the
// portal on departure. The departure lane is held until the clear point.
void GameSimulation::updateDockPilotShips()
{
    using game::docking::traffic::DockTrafficPhase;
    if (!m_dockServiceTraffic)
        return;
    const StaticObject* dock = nullptr;
    const StaticObject* base = nullptr;
    for (const auto& [id, obj] : m_staticObjects)
    {
        if (!obj.attachedToHub)
            continue;
        if (obj.hubModuleId == DockModuleId)
            dock = &obj;
        else if (obj.hubModuleId == DockBaseModuleId)
            base = &obj;
    }
    if (!dock || !base)
        return;

    const double now = m_serverTimelineClock.timeSeconds();
    const glm::dvec3 origin = world::coordinates::fullMeters(dock->worldPosition);
    const glm::dmat3 fixedBasis(base->orientation);
    const glm::dmat3 toFixed = glm::transpose(fixedBasis);
    const glm::dvec3 dockVelocity(dock->linearVelocity);
    auto& traffic = *m_dockServiceTraffic;
    auto& dispatcher = traffic.dispatcher();
    const auto envelope = game::traffic::dock::dockingEnvelope(
        game::traffic::dock::trafficShipClass(game::traffic::dock::TrafficShipKind::Cobrett));

    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr || !isPlayerControlled(id) ||
            !game::simulation::sameRuntimeSystem(shipPtr->core().transform().motion.systemId, dock->systemId))
            continue;
        auto& tr = shipPtr->core().transform();
        const glm::dvec3 p = toFixed * (tr.fullWorldMeters() - origin);
        const glm::dvec3 v = toFixed * (tr.motion.worldVelocityMps - dockVelocity);
        const auto status = dispatcher.status(id.value);
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
                        std::cerr << "[DockTraffic] pilot " << id.value << " docking request "
                                  << (ok ? "accepted" : "rejected") << " at z=" << p.z << "\n";
                    }
                    break;
                case DockTrafficPhase::Queued:
                case DockTrafficPhase::EntryCleared:
                    if (glm::length(p) > 4000.0)
                    {
                        dispatcher.cancel(id.value, now);
                        std::cerr << "[DockTraffic] pilot " << id.value << " left the zone, request cancelled\n";
                    }
                    else if (status.phase == DockTrafficPhase::EntryCleared &&
                             radial < 60.0 && p.z > -800.0 && p.z < -150.0 && glm::length(v) < 15.0)
                    {
                        game::traffic::dock::TrafficPose pose;
                        pose.present = true;
                        pose.position = p;
                        pose.basis = toFixed * glm::dmat3(glm::mat3(tr.orientation));
                        pose.velocity = v;
                        if (traffic.beginStationCapture(id.value, game::traffic::dock::TrafficShipKind::Cobrett, pose, now))
                        {
                            m_dockStationControlledShips.insert(id);
                            std::cerr << "[DockTraffic] pilot " << id.value << " captured by station control -> "
                                      << status.padId << "\n";
                        }
                    }
                    break;
                case DockTrafficPhase::DepartureCleared:
                    if (p.z <= -2400.0 || glm::length(p) > 3000.0)
                    {
                        dispatcher.reportClear(id.value, now);
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
            // Released at the portal after the corridor out: physics again.
            m_dockStationControlledShips.erase(id);
            std::cerr << "[DockTraffic] pilot " << id.value << " released at the portal\n";
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
