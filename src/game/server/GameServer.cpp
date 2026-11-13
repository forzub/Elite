#include <atomic>
#include <chrono>
#include <thread>

#include "src/game/system_map/PlanetBodyOrientation.h"
#include "src/game/navigation/globalflight/BodyTerrainProbe.h"
#include "src/world/celestial/surface/BodySurfaceFieldLoader.h"
#include "GameServer.h"
#include "src/core/RuntimeTrace.h"
#include "src/game/network/ReplicationSnapshotMerge.h"
#include <type_traits>
#include "src/game/network/ClientMessage.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "src/world/coordinates/WorldPosition.h"
#include <cmath>
#include <functional>
#include <unordered_map>
#include <utility>
#include <thread>
#include <iterator>

#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/celestial/WorldSitesCatalog.h"
#include "src/game/navigation/rules/InterstellarJumpRoute.h"
#include "src/game/navigation/rules/JumpSumps.h"
#include "src/game/navigation/rules/NavDataBanks.h"
#include "src/game/navigation/rules/JumpRules.h"
#include <random>
#include "src/game/scene/GameSceneSetup.h"
#include "src/game/server/ServerTickProfile.h"
#include "src/game/economy/EconomyModel.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include <map>
#include <tuple>
#include "src/game/world_state/InitialWorldState.h"
#include "src/game/navigation/GalaxyNavigationConfig.h"
#include "src/game/navigation/PlayerSpatialDomainResolver.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/debug/DamagedEnginePreset.h"
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "src/world/coordinates/WorldPosition.h"





namespace
{
std::atomic<bool> g_devCommandsAllowed {false};
bool devCommandAccepted(const char* what, EntityId ship)
{
    if (g_devCommandsAllowed.load())
        return true;
    std::cerr << "[Server] DEV command '" << what << "' from ship " << ship.value << " REJECTED (dev commands off)" << std::endl;
    return false;
}
} // namespace

void GameServer::setDevCommandsAllowed(bool allowed) noexcept { g_devCommandsAllowed = allowed; }
bool GameServer::devCommandsAllowed() noexcept { return g_devCommandsAllowed.load(); }

GameServer::GameServer(std::size_t bootstrapPlayerSlotCount)
    : m_diagnostics{}
    , m_simulation(m_diagnostics)
{
        m_simulation.setTerrainHeightProvider(
            [this](const std::string& bodyId, const glm::dvec3& worldDirection, GameSimulation::TerrainSample& ground) {
                return terrainHeight(bodyId, worldDirection, ground);
            });
        m_simulation.setBodyOrientationProvider([this](const std::string& bodyId, glm::dmat3& worldFromBody) {
            const auto* snapshot = celestialSnapshotForSystem(m_appliedSimulationContextSystemId);
            if (!snapshot)
                return false;
            for (const auto& b : snapshot->bodies)
                if (b.id == bodyId)
                {
                    const auto o = game::system_map::makePlanetTextureOrientation(b.axialTiltDeg, b.axisNodeDeg,
                                                                                  b.rotationPhaseRad, b.textureLongitudeOffsetDeg);
                    worldFromBody = glm::dmat3(glm::dvec3(o.prime), glm::dvec3(o.north), glm::dvec3(o.east));
                    return true;
                }
            return false;
        });

        m_universeClock.reset();
        m_universeClock.setTimeScale(
            m_debugFastUniverseTimeScale
        );

        m_lastUniverseTimeSeconds =
            m_universeClock.timeSeconds();

        const auto navigationConfig =
            game::navigation::GalaxyNavigationConfig::loadFromRuntimeOrSource(
                "assets/data/navigation/navigation_grid.json",
                "src/assets/data/navigation/navigation_grid.json"
            );
        m_systemMembershipRadiusAu =
            navigationConfig.systemMembershipRadiusAu;

        const bool atlasLoaded =
            m_starAtlas.loadFromRuntimeOrSource();

        if (!atlasLoaded)
        {
            std::cerr
                << "[GameServer] galaxy details catalog was not loaded\n";
        }

        game::world_state::InitialWorldState initialWorldState;
        const bool initialWorldLoaded =
            game::world_state::loadInitialWorldStateWithFallbacks(
                initialWorldState
            );

        if (!initialWorldLoaded)
        {
            throw std::runtime_error(
                "authoritative initial world state is missing or invalid"
            );
        }

        for (const auto& systemState : initialWorldState.systemStates)
        {
            // World-state system ids are PHYSICAL StarAtlas ids. The legacy
            // political GalaxyDatabase uses an unrelated id space and is
            // never joined by int (see SpaceState / check_system_id_namespaces).
            if (!m_starAtlas.findSystem(systemState.systemId))
            {
                throw std::runtime_error(
                    "system_states references a physical system absent from StarAtlas: " +
                    std::to_string(systemState.systemId)
                );
            }

            m_systemJurisdictions[systemState.systemId] =
                systemState.jurisdiction;
        }

        const int initialSystemId =
            initialWorldState.playerStart.systemId;

        if (!m_starAtlas.findSystem(initialSystemId))
        {
            throw std::runtime_error(
                "player_start references a physical system absent from StarAtlas"
            );
        }

        m_celestialRuntimes.initialize(m_starAtlas);




const double universeTime =
    m_universeClock.timeSeconds();


std::unordered_map<std::string, glm::dvec3>
    currentCelestialPositionsAu;
std::unordered_map<std::string, glm::dvec3>
    currentCelestialVelocitiesAuPerSecond;

if (const auto* celestial =
        celestialSnapshotForSystem(
            initialSystemId
        ))
{
    for (const auto& state : celestial->bodies)
    {
        currentCelestialPositionsAu[state.id] =
            state.positionAu;
        currentCelestialVelocitiesAuPerSecond[state.id] =
            state.velocityAuPerSecond;
    }
}

m_simulation.setOrbitalUniverseTimeSeconds(universeTime);
m_simulation.setCelestialBodyKinematicStateAu(
    initialSystemId,
    currentCelestialPositionsAu,
    currentCelestialVelocitiesAuPerSecond
);











        m_simulation.buildInitialScene(initialWorldState);

        applyCelestialOrbitParentParameters(initialSystemId);
        m_gravityParametersAppliedSystems.insert(initialSystemId);
        m_initialSimulationSystemId = initialSystemId;
        materializeWorldSiteHubs(initialSystemId);

        // Готовим хабы, станции и reference frames до размещения игрока.
        // Это не полный update и не создаёт грязный стартовый snapshot.
        m_simulation.prepareReferenceFramesForSpawn();

        game::navigation::ReferenceFrame playerStartFrame;
        playerStartFrame.type =
            game::navigation::ReferenceFrameType::OrbitalHub;
        playerStartFrame.systemId =
            initialWorldState.playerStart.systemId;
        playerStartFrame.hubId =
            initialWorldState.playerStart.hubId;
        playerStartFrame.localOffsetMeters =
            initialWorldState.playerStart.localOffsetMeters;

        if (!m_simulation.placeShipInReferenceFrame(
                m_simulation.playerId(),
                playerStartFrame))
        {
            throw std::runtime_error(
                "validated player_start reference frame could not be resolved"
            );
        }

        // EXPLICIT DIAGNOSTIC: one damaged INSTALLED engine in the
        // authoritative simulated ship, before snapshots, pilot admission
        // and route planning. This is not a client-only capability override.
        // Without the environment variable, the ordinary ship is untouched.
        if (const char* raw =
                std::getenv("ELITE_SHIP_DAMAGE_PRESET"))
        {
            if (*raw)
            {
                using namespace game::ship::debug;
                const auto preset = parseDamagedEnginePreset(raw);
                bool applied = false;
                std::string engineId;
                std::string outcome;
                if (preset == DamagedEnginePreset::None)
                {
                    outcome = "unknown preset (expected aft-50, aft-80, aft-failed)";
                }
                else if (Ship* ship =
                             m_simulation.getShip(m_simulation.playerId()))
                {
                    const auto& desc = ship->core().descriptor();
                    const std::size_t slot =
                        firstAftMainSlot(desc.physicalControlModel);
                    const auto& bindings =
                        desc.physicalControl.slotModuleIds;
                    if (ship->core().state().actuators.source !=
                            game::ship::state::ShipActuatorStateSource::ModuleRuntime)
                    {
                        outcome = "layout has no real installed engine bindings";
                    }
                    else if (slot >= bindings.size())
                    {
                        outcome = "aft-main slot has no installed module binding";
                    }
                    else
                    {
                        engineId = bindings[slot];
                        const auto def = std::find_if(
                            desc.modules.begin(), desc.modules.end(),
                            [&engineId](const auto& module)
                            {
                                return module.moduleId == engineId;
                            });
                        const auto* installed =
                            static_cast<const world::modules::ObjectModuleRuntime&>(
                                ship->core().moduleRuntime()
                            ).findModule(engineId);
                        if (def == desc.modules.end() ||
                            !def->engine || !installed)
                        {
                            outcome = "physical aft engine module not found";
                        }
                        else
                        {
                            applied =
                                ship->core().moduleRuntime().injectEquipmentFailure(
                                    engineId, engineDamageForPreset(preset)
                                );
                            outcome = applied
                                ? "applied before planning"
                                : "installed engine did not accept damage";
                        }
                    }
                }
                else
                {
                    outcome = "primary player ship absent";
                }

                const std::string report =
                    std::string("[ShipDamagePreset] preset=") +
                    raw +
                    " status=" + (applied ? "APPLIED" : "FAILED") +
                    " module=" + engineId +
                    " internal_failure_target=" +
                    std::to_string(engineDamageForPreset(preset)) +
                    " detail=" + outcome;
                std::cerr << report << '\n';

#ifdef _WIN32
                // Only the explicit diagnostic writes additional files.
                // Never create output in source/build folders.
                std::error_code logError;
                std::filesystem::create_directories(
                    "D:/tmp", logError);
                std::ofstream log(
                    "D:/tmp/elite_engine_damage_preset.log",
                    std::ios::app);
                if (log)
                    log << report << '\n';
#endif
            }
        }

        std::vector<EntityId> bootstrapPlayerEntities;
        bootstrapPlayerEntities.push_back(m_simulation.playerId());

        // Dedicated multiplayer bootstrap temporarily materializes a small
        // server-owned pool of player-eligible ships. This is deliberately not
        // account ownership: M8E only needs deterministic admission capacity
        // without hijacking arbitrary NPCs. Local/embedded play requests one
        // slot and therefore preserves the historical single-player scene.
        const std::size_t requestedPlayerSlots =
            std::max<std::size_t>(1, bootstrapPlayerSlotCount);

        if (requestedPlayerSlots > 1)
        {
            Ship* primaryShip =
                m_simulation.getShip(m_simulation.playerId());

            if (!primaryShip)
            {
                throw std::runtime_error(
                    "bootstrap player ship disappeared before admission pool setup"
                );
            }

            constexpr double BootstrapPlayerSpacingMeters = 50.0;

            for (std::size_t slotIndex = 1;
                 slotIndex < requestedPlayerSlots;
                 ++slotIndex)
            {
                // Sequence around the authored primary spawn:
                // +50, -50, +100, -100 ... hub-local X meters.
                const std::size_t ring = (slotIndex + 1) / 2;
                const double side = (slotIndex % 2 == 1) ? 1.0 : -1.0;

                game::navigation::ReferenceFrame slotFrame = playerStartFrame;
                slotFrame.localOffsetMeters.x +=
                    side *
                    static_cast<double>(ring) *
                    BootstrapPlayerSpacingMeters;

                ShipInitData slotInitData;
                slotInitData.visual.shipType = "Cobra MK1";
                slotInitData.visual.shipName =
                    "Bootstrap Player " + std::to_string(slotIndex + 1);
                slotInitData.registry.instanceId =
                    900000u + static_cast<ShipInstanceId>(slotIndex);
                slotInitData.registry.ownerName = "Unassigned Player Slot";
                slotInitData.registry.registrationId =
                    "BOOT-PL-" + std::to_string(slotIndex + 1);
                slotInitData.registry.shipRole = ShipRoleType::Civilian;

                const EntityId slotId =
                    m_simulation.spawnShip(
                        ShipRole::Player,
                        playerStartFrame.systemId,
                        primaryShip->core().descriptor(),
                        primaryShip->core().transform().fullWorldMeters(),
                        slotInitData,
                        primaryShip->core().transform().orientation
                    );

                if (slotId.value == 0 ||
                    !m_simulation.placeShipInReferenceFrame(slotId, slotFrame))
                {
                    throw std::runtime_error(
                        "failed to materialize bootstrap player admission slot"
                    );
                }

                bootstrapPlayerEntities.push_back(slotId);
            }
        }

        // Build persistent universe identity before any connection/session is
        // admitted. Every materialized ship, human or AI-controlled, has one
        // stable ShipInstanceId. Runtime EntityId remains only a materialized
        // simulation handle.
        for (const auto& [entityId, shipPtr] : m_simulation.ships())
        {
            if (!shipPtr)
                continue;

            const auto& core = shipPtr->core();
            const auto& registration = core.registry();

            game::server::ShipInstanceRecord record;
            record.instanceId = registration.instanceId;
            record.materializedEntityId = entityId;
            record.typeId = core.desc().typeId;
            record.catalogShipId = core.desc().catalog.passportId;
            record.roleType = registration.shipRole;
            record.ownerActor = registration.ownerActor;
            record.name = core.visualIdentity().shipName;
            record.registrationId = registration.registrationId;

            if (!m_shipInstances.registerMaterialized(std::move(record)))
            {
                throw std::runtime_error(
                    "ship instance registry rejected zero/duplicate persistent identity"
                );
            }
        }

        // Bootstrap players are persistent identities assigned to persistent
        // ships. Session connections are created later and only authenticate
        // access to these player identities. No NPC is promoted by connection.
        for (const EntityId entityId : bootstrapPlayerEntities)
        {
            const Ship* ship = m_simulation.getShip(entityId);
            if (!ship)
            {
                throw std::runtime_error(
                    "bootstrap player entity disappeared before identity registration"
                );
            }

            const auto& registration = ship->core().registry();
            const PlayerId playerId = m_players.create(
                registration.instanceId,
                registration.ownerActor
            );

            if (!playerId || !m_controls.bindHuman(playerId, entityId))
            {
                throw std::runtime_error(
                    "failed to bind bootstrap player identity to ship control"
                );
            }

            // Initial human pilots are independent individuals: no organization
            // membership is implied. Their starter ship is legally self-owned,
            // while runtime control remains a separate ControlRegistry concern.
            if (!m_shipOwnership.assign(
                    registration.instanceId,
                    game::server::ShipOwnerRef::player(playerId)))
            {
                throw std::runtime_error(
                    "failed to assign bootstrap player ship self-ownership"
                );
            }

            if (entityId == m_simulation.playerId())
                m_primaryPlayerId = playerId;
        }

        if (!m_primaryPlayerId)
        {
            throw std::runtime_error(
                "primary bootstrap player identity was not registered"
            );
        }

        game::server::ServerTimeContext initialTime;
        initialTime.serverTick = 0;
        initialTime.universeTimeSeconds = universeTime;
        initialTime.universeTimeScale = m_universeClock.timeScale();
        initialTime.universeTimeSimulation =
            m_universeClock.simulationMode();

        m_simulation.update(initialTime);






        m_lastSnapshot =
            m_simulation.buildReplicationSnapshot(0);
        populateClientSessionSnapshot(m_lastSnapshot);
        m_canonicalReplicationSnapshot =
            game::network::materializeCanonicalReplicationSnapshot(
                nullptr,
                m_lastSnapshot
            );


}








world::celestial::PlayerNavigationState
GameServer::navigationStateForEntity(EntityId entityId) const
{
    world::celestial::PlayerNavigationState navigation;

    const Ship* ship = m_simulation.getShip(entityId);
    if (!ship)
        return navigation;

    const ShipTransform tr =
        m_simulation.presentationShipTransform(entityId);

    const auto spatialDomain =
        game::navigation::resolvePlayerSpatialDomain(
            m_starAtlas.systems(),
            tr.motion.systemId,
            tr.worldPosition,
            m_systemMembershipRadiusAu
        );

    if (spatialDomain.valid)
    {
        navigation.currentSystemId =
            spatialDomain.currentSystemId;
        navigation.worldPosition =
            spatialDomain.worldPosition;
        navigation.systemLocalMeters =
            spatialDomain.systemLocalMeters;
        navigation.systemLocalAu =
            spatialDomain.systemLocalAu;
    }
    else
    {
        // A catalog/source mismatch must not invent a transfer. Preserve the
        // authoritative entity membership and position as a safe fallback.
        navigation.currentSystemId = tr.motion.systemId;
        navigation.worldPosition = tr.worldPosition;
        navigation.systemLocalMeters =
            world::coordinates::fullMeters(tr.worldPosition);
        navigation.systemLocalAu =
            navigation.systemLocalMeters /
            world::celestial::MetersPerAu;
    }

    navigation.orientation = tr.orientation;
    navigation.forward = tr.forward();
    navigation.up = tr.up();
    return navigation;
}


const world::celestial::CelestialSystemSnapshot*
GameServer::celestialSnapshotForSystem(int systemId) const
{
    return m_celestialRuntimes.resolve(
        systemId,
        m_universeClock.timeSeconds()
    );
}


void GameServer::switchSimulationContext(int systemId)
{
    std::unordered_map<std::string, glm::dvec3> celestialPositionsAu;
    std::unordered_map<std::string, glm::dvec3> celestialVelocitiesAuPerSecond;
    if (const auto* celestial = celestialSnapshotForSystem(systemId))
        for (const auto& state : celestial->bodies)
        {
            celestialPositionsAu[state.id] = state.positionAu;
            celestialVelocitiesAuPerSecond[state.id] = state.velocityAuPerSecond;
        }
    m_simulation.setCelestialBodyKinematicStateAu(systemId, celestialPositionsAu, celestialVelocitiesAuPerSecond);
    if (m_gravityParametersAppliedSystems.insert(systemId).second)
        applyCelestialOrbitParentParameters(systemId);
    m_appliedSimulationContextSystemId = systemId;
}

void GameServer::materializeWorldSiteHubs(int startSystemId)
{
    world::celestial::WorldSitesCatalog sites;
    if (!sites.loadFromRuntimeOrSource())
        return;
    int made = 0;
    int systems = 0;
    for (const auto& summary : m_starAtlas.systems())
    {
        const int systemId = summary.id;
        if (systemId == startSystemId)
            continue;
        const auto* system = m_starAtlas.findSystem(systemId);
        if (!system)
            continue;
        // body -> its most populated site (an orbital one first: it is a station already)
        std::map<std::string, const world::celestial::WorldSite*> best;
        for (const auto& s : sites.sitesInSystem(systemId))
        {
            const auto* b = [&]() -> const world::celestial::CelestialBodyDefinition* {
                for (const auto& body : system->bodies)
                    if (body.id == s.parentBodyId)
                        return &body;
                return nullptr;
            }();
            if (!b || b->radiusKm <= 0.0 || b->gravitationalParameterM3s2 <= 0.0)
                continue;   // a belt / trojan reservoir, not a body a hub can orbit
            const bool orbit = s.placement == world::celestial::WorldSitePlacementKind::Orbit && s.altitudeKm > 0.0;
            auto& cur = best[s.parentBodyId];
            const bool curOrbit = cur && cur->placement == world::celestial::WorldSitePlacementKind::Orbit && cur->altitudeKm > 0.0;
            if (!cur || (orbit && !curOrbit) || (orbit == curOrbit && s.population > cur->population))
                cur = &s;
        }
        if (best.empty())
            continue;
        switchSimulationContext(systemId);
        ++systems;
        for (const auto& [bodyId, s] : best)
        {
            const world::celestial::CelestialBodyDefinition* body = nullptr;
            for (const auto& b : system->bodies)
                if (b.id == bodyId)
                    body = &b;
            const double radiusM = body->radiusKm * 1000.0;
            const double floorM = game::navigation::rules::floorAltitudeM(radiusM, std::max(radiusM, body->atmosphereOuterRadiusKm * 1000.0));
            game::world_state::InitialWorldStateOrbitalHub hub;
            hub.id = s->id;
            hub.name = s->name;
            hub.owner = s->operatorName.empty() ? s->polityName : s->operatorName;
            hub.systemId = systemId;
            hub.parentBodyId = bodyId;
            hub.mapObjectModuleId = s->id + ".station";
            hub.motion.type = "parent_orbit";
            hub.motion.orbitalPeriodPolicy = world::orbits::OrbitalPeriodPolicy::Kepler;
            hub.motion.orbitalPeriodSeconds = 0.0;
            // a surface / belt site: its hub on a low orbit just above the J floor
            hub.motion.altitudeKm = s->placement == world::celestial::WorldSitePlacementKind::Orbit && s->altitudeKm > 0.0
                ? std::max(s->altitudeKm, floorM / 1000.0 + 100.0)
                : floorM / 1000.0 + 300.0;
            hub.motion.selfRotationPeriodSeconds = 180.0;
            hub.motion.inclinationDeg = s->inclinationDeg;
            const auto h = std::hash<std::string> {}(s->id);
            hub.motion.longitudeOfAscendingNodeDeg = static_cast<double>(h % 360);
            hub.motion.initialPhaseDeg = static_cast<double>((h / 360) % 360);
            game::world_state::InitialWorldStateHubModule station;
            station.id = hub.mapObjectModuleId;
            station.type = "command_station";
            station.name = s->name;
            hub.modules.push_back(station);
            if (game::scene::spawnOrbitalHub(m_simulation, hub))
                ++made;
            else
                std::cerr << "[WorldSiteHubs] " << s->id << " on " << bodyId << ": not created" << std::endl;
        }
    }
    switchSimulationContext(startSystemId);
    std::cerr << "[WorldSiteHubs] " << made << " hubs in " << systems << " inhabited systems (one per body with world sites)" << std::endl;
}

/*
    DEV start point (automated runs; author 2026-11-11): ELITE_DEV_START
      hub:<hub id>[@<km>]      normal flight <km> (default 50) above the hub
      body:<name>[@<system>]   orbit of a body at its J floor + 100 km, as a jump exit; <name> = the
                               body's descriptor file stem without planet_ / moon_ / dwarf_planet_
                               (ASCII: moon, phobos, jupiter, eps_eri_b), searched in <system> or the
                               current system first, then every system
      system:<id>              the system's exit planet (the jump exit rule)
    Applied once, ~3 s of server time after the start (session, hub frames settled).
*/
void GameServer::applyDevStartPoint(double dt)
{
    static const std::string spec = [] {
        const char* v = std::getenv("ELITE_DEV_START");
        return std::string(v ? v : "");
    }();
    static bool done = false;
    static double waited = 0.0;
    if (done || spec.empty())
        return;
    waited += std::max(0.0, dt);
    const EntityId ship = m_simulation.playerId();
    if (waited < 3.0 || !m_simulation.getShip(ship))
        return;
    done = true;
    const auto colon = spec.find(':');
    const std::string kind = spec.substr(0, colon);
    std::string arg = colon == std::string::npos ? std::string() : spec.substr(colon + 1);
    std::string after;
    if (const auto at = arg.find('@'); at != std::string::npos)
    {
        after = arg.substr(at + 1);
        arg.resize(at);
    }
    bool ok = false;
    if (kind == "hub")
        ok = m_simulation.devPlaceShipNearHub(ship, arg, after.empty() ? 50.0 : std::atof(after.c_str()));
    else if (kind == "system")
        ok = devPlaceShipInSystem(ship, std::atoi(arg.c_str()), 0);
    else if (kind == "deep")
    {
        // deep:<n>[@<AU from the centre>] - the n-th deep-space place (a swarm between the systems), at rest
        const int place = world::celestial::StarAtlasDatabase::DeepSpaceIdBase + std::atoi(arg.c_str());
        if (m_starAtlas.findSystemSummary(place))
            ok = m_simulation.placeShipInDeepSpace(ship, place, glm::dvec3(after.empty() ? 1000.0 : std::atof(after.c_str()), 0.0, 0.0) * 1.495978707e11);
    }
    else if (kind == "approach")
    {
        // approach:<systemId>[@<AU from the star>[:c<k>]] - the system's approaches (JUMP_MECHANICS 10h), at rest; :c<k> = on
        // the axis of the shell's corridor k (1-based) as it lies now, else along +x
        const int system = std::atoi(arg.c_str());
        const int place = world::celestial::StarAtlasDatabase::approachIdOf(system);
        double au = 20000.0;
        int corridor = 0;
        if (!after.empty())
        {
            au = std::atof(after.c_str());
            if (const auto c = after.find(":c"); c != std::string::npos)
                corridor = std::atoi(after.c_str() + c + 2);
        }
        glm::dvec3 dir(1.0, 0.0, 0.0);
        if (const auto* shell = game::navigation::jump::approachShellOf(m_starAtlas, system);
            shell && corridor >= 1 && corridor <= static_cast<int>(shell->corridors.size()))
            dir = game::navigation::jump::corridorDirAt(*shell, shell->corridors[static_cast<std::size_t>(corridor - 1)],
                                                        m_universeClock.timeSeconds());
        if (m_starAtlas.findSystemSummary(place))
            ok = m_simulation.placeShipInDeepSpace(ship, place, dir * au * 1.495978707e11);
    }
    else if (kind == "point")
    {
        // point:<x>,<y>,<z> - AU from the star of the ship's system
        glm::dvec3 au(0.0);
        if (std::sscanf(arg.c_str(), "%lf,%lf,%lf", &au.x, &au.y, &au.z) == 3)
            ok = m_simulation.devPlaceShipAtPoint(ship, au);
    }
    else if (kind == "body")
    {
        const auto stemOf = [](const std::string& path) {
            const std::size_t slash = path.find_last_of("/\\");
            std::string stem = slash == std::string::npos ? path : path.substr(slash + 1);
            if (const std::size_t dot = stem.rfind('.'); dot != std::string::npos)
                stem.resize(dot);
            for (const char* prefix : {"dwarf_planet_", "planet_", "moon_", "dwarf_"})
                if (stem.rfind(prefix, 0) == 0)
                    return stem.substr(std::char_traits<char>::length(prefix));
            return stem;
        };
        std::vector<int> order;
        if (!after.empty())
            order.push_back(std::atoi(after.c_str()));
        else if (const Ship* s = m_simulation.getShip(ship))
            order.push_back(s->core().transform().motion.systemId);
        for (const auto& summary : m_starAtlas.systems())
            order.push_back(summary.id);
        for (int systemId : order)
        {
            const auto* system = m_starAtlas.findSystem(systemId);
            if (!system)
                continue;
            for (const auto& b : system->bodies)
                if (!b.sourcePath.empty() && b.radiusKm > 0.0 && stemOf(b.sourcePath) == arg)
                {
                    ok = m_simulation.devPlaceShipInSystem(ship, systemId, system->name, b.id);
                    std::cerr << "[DevStart] body " << arg << " -> " << b.id << " (system " << systemId << ")" << std::endl;
                    break;
                }
            if (ok)
                break;
        }
    }
    std::cerr << "[DevStart] ELITE_DEV_START=" << spec << (ok ? " applied" : " NOT applied (unknown target)") << std::endl;
    // ELITE_DEV_LANES=<system>:<corridor>:<age years>[:<error deg>],... - lane records surveyed that long ago (the true
    // corridor direction then, plus the error turned about the corridor's drift axis)
    if (const char* lanes = std::getenv("ELITE_DEV_LANES"); lanes && *lanes)
    {
        namespace jj = game::navigation::jump;
        std::vector<jj::LaneRecord> records;
        std::string all = lanes;
        std::size_t pos = 0;
        while (pos < all.size())
        {
            std::size_t end = all.find(',', pos);
            if (end == std::string::npos)
                end = all.size();
            int system = -1, corridor = 0;
            double ageYears = 0.0, errorDeg = 0.0;
            if (std::sscanf(all.substr(pos, end - pos).c_str(), "%d:%d:%lf:%lf", &system, &corridor, &ageYears, &errorDeg) >= 3)
                if (const auto* shell = jj::approachShellOf(m_starAtlas, system);
                    shell && corridor >= 1 && corridor <= static_cast<int>(shell->corridors.size()))
                {
                    const auto& c = shell->corridors[static_cast<std::size_t>(corridor - 1)];
                    jj::LaneRecord r;
                    r.systemId = system;
                    r.corridor = corridor;
                    r.surveyUniverseS = m_universeClock.timeSeconds() - ageYears * 365.25 * 86400.0;
                    const glm::dvec3 d = jj::corridorDirAt(*shell, c, r.surveyUniverseS);
                    const double e = glm::radians(errorDeg);
                    r.dir = glm::normalize(d * std::cos(e) + glm::cross(c.axis, d) * std::sin(e));
                    r.errorDeg = errorDeg;
                    r.source = "DEV";
                    records.push_back(r);
                }
            pos = end + 1;
        }
        if (const char* toCarrier = std::getenv("ELITE_DEV_LANES_ON_CARRIER"); toCarrier && *toCarrier)
        {
            // DEV: the same records on a data carrier "found" aboard, not in the computer
            jj::DataCarrier c;
            c.label = toCarrier;
            c.records = records;
            m_simulation.setDataCarriers(ship, jj::formatDataCarriers({c}));
            std::cerr << "[DevStart] ELITE_DEV_LANES: " << records.size() << " lane record(s) on a carrier '" << toCarrier << "'" << std::endl;
        }
        else
        {
            m_simulation.setLaneRecords(ship, jj::formatLaneRecords(records));
            std::cerr << "[DevStart] ELITE_DEV_LANES: " << records.size() << " lane record(s)" << std::endl;
        }
    }
}

bool GameServer::devPlaceShipInSystem(EntityId shipId, int systemId, int ordinal)
{
    const auto* system = m_starAtlas.findSystem(systemId);
    if (!system)
        return false;
    std::vector<std::string> exits;
    for (const auto& [hubId, hub] : m_simulation.orbitalHubs())
        if (hub.systemId == systemId && std::find(exits.begin(), exits.end(), hub.parentBodyId) == exits.end())
            exits.push_back(hub.parentBodyId);
    std::sort(exits.begin(), exits.end());
    if (exits.empty())
    {
        std::vector<const world::celestial::CelestialBodyDefinition*> planets;
        for (const auto& b : system->bodies)
            if (b.type == world::celestial::BodyType::Planet && b.radiusKm >= 1000.0)
                planets.push_back(&b);
        std::sort(planets.begin(), planets.end(), [](const auto* a, const auto* b) { return a->massKg > b->massKg; });
        for (const auto* p : planets)
            exits.push_back(p->id);
    }
    const std::string exit = exits.empty() ? std::string() : exits[static_cast<std::size_t>(ordinal) % exits.size()];
    return m_simulation.devPlaceShipInSystem(shipId, systemId, system->name, exit);
}

void GameServer::applyCelestialOrbitParentParameters(int systemId)
{

    const auto* system =
        m_starAtlas.findSystem(systemId);

    if (!system)
        return;

    for (const auto& body : system->bodies)
    {
        if (body.gravitationalParameterM3s2 <= 0.0)
            continue;

        if (body.radiusKm <= 0.0)
            continue;

        const double radiusMeters =
            body.radiusKm * 1000.0;

        m_simulation.setCelestialBodyGravityParameters(
            systemId,
            body.id,
            radiusMeters,
            body.gravitationalParameterM3s2,
            body.atmosphereOuterRadiusKm * 1000.0,
            body.surfacePressurePa
        );

        m_simulation.updateStaticObjectOrbitParentParameters(
            systemId,
            body.id,
            radiusMeters,
            body.gravitationalParameterM3s2
        );
    }

    m_simulation.setJumpSumps(systemId, game::navigation::jump::jumpSumpsOfSystem(*system));
    m_appliedSimulationContextSystemId = systemId;
}
















namespace
{

// EXPLICIT DIAGNOSTIC (damaged stage): ELITE_SHIP_DAMAGE_AT=
// "T MODULE TARGET01[;T MODULE TARGET01...]" injects a real installed
// equipment fault (the same injectEquipmentFailure path as the server DEV
// command) into the player ship T seconds after it first moved faster than
// 1 m/s - a capability change DURING a route (T < 0: at once, before the
// route). MODULE "reactors-toggle"
// switches the propulsion reactors instead. Unset: nothing happens.
struct TimedDamageDiagnostic
{
    struct Event { double at; std::string module; double target01; bool done; };
    bool parsed = false;
    std::vector<Event> events;
    double movingSeconds = -1.0;
};

void advanceTimedDamageDiagnostic(TimedDamageDiagnostic& d, Ship* ship, double dt)
{
    if (!d.parsed)
    {
        d.parsed = true;
        if (const char* raw = std::getenv("ELITE_SHIP_DAMAGE_AT"))
        {
            std::stringstream all(raw);
            std::string item;
            while (std::getline(all, item, ';'))
            {
                std::stringstream one(item);
                TimedDamageDiagnostic::Event e {};
                if (one >> e.at >> e.module >> e.target01)
                    d.events.push_back(e);
            }
        }
    }
    if (d.events.empty() || !ship)
        return;
    auto& core = ship->core();
    if (d.movingSeconds < 0.0)
    {
        // T < 0: damaged BEFORE the route (applied at once, ship at rest).
        for (auto& e : d.events)
            if (!e.done && e.at < 0.0 && e.module != "reactors-toggle")
            {
                e.done = true;
                const bool applied =
                    core.moduleRuntime().injectEquipmentFailure(e.module, e.target01);
                core.refreshRuntimeState();
                std::cerr << "[ShipDamageAt] t_moving=before module=" << e.module
                          << " target=" << e.target01
                          << " status=" << (applied ? "APPLIED" : "FAILED") << '\n';
            }
        if (glm::length(core.transform().motion.localVelocityMps) <= 1.0)
            return;
        d.movingSeconds = 0.0;
    }
    d.movingSeconds += std::max(0.0, dt);
    for (auto& e : d.events)
        if (!e.done && d.movingSeconds >= e.at)
        {
            e.done = true;
            // MODULE "reactors-toggle": the propulsion reactors switch
            // (off, or on again with the real restart time).
            if (e.module == "reactors-toggle")
            {
                core.togglePropulsionReactors();
                std::cerr << "[ShipDamageAt] t_moving=" << d.movingSeconds
                          << " propulsion reactors toggled\n";
                continue;
            }
            const bool applied =
                core.moduleRuntime().injectEquipmentFailure(e.module, e.target01);
            core.refreshRuntimeState();
            std::cerr << "[ShipDamageAt] t_moving=" << d.movingSeconds
                      << " module=" << e.module << " target=" << e.target01
                      << " status=" << (applied ? "APPLIED" : "FAILED") << '\n';
        }
}

} // namespace

void GameServer::update(double dt)
{
    static TimedDamageDiagnostic timedDamage;
    advanceTimedDamageDiagnostic(
        timedDamage, m_simulation.getShip(m_simulation.playerId()), dt);
    // Capture the passive-trajectory seed before the accelerated clock is
    // advanced. At this point celestial bodies, hubs, reference frames and
    // ships all belong to the same last completed authoritative epoch.
    if (m_pendingUniverseTrajectoryDiagnosticEntry)
    {
        const bool diagnosticReady =
            m_simulation.beginUniverseTrajectoryDiagnostic(
                m_pendingUniverseTrajectoryDiagnosticEpochSeconds
            );

        m_pendingUniverseTrajectoryDiagnosticEntry = false;

        if (!diagnosticReady)
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] activation cancelled: "
                << "not every eligible ship could enter the diagnostic branch\n";

            /*
                Failure is itself a timeline transition: setSimulationMode(false)
                rewinds UniverseClock to the real epoch. Route it through the
                normal setter so revision fencing is published as well.
            */
            setDebugUniverseTimeSimulation(
                false,
                m_universeClock.configuredTimeScale()
            );
        }
    }

    m_universeClock.update(dt);
    m_serverTick++;
    applyDevStartPoint(dt);

    const double universeTime =
        m_universeClock.timeSeconds();

    game::server::ServerTimeContext time;
    time.serverTick = m_serverTick;
    time.serverDeltaSeconds = std::max(0.0, dt);
    time.universeTimeSimulation =
        m_universeClock.simulationMode();
    time.gameplayDeltaSeconds =
        time.universeTimeSimulation
            ? 0.0
            : time.serverDeltaSeconds;
    time.universeTimeSeconds = universeTime;
    time.universeDeltaSeconds =
        universeTime - m_lastUniverseTimeSeconds;
    time.universeTimeScale =
        m_universeClock.timeScale();

    m_lastUniverseTimeSeconds = universeTime;

    // 1. Apply commands. In accelerated universe-time diagnostics ships are
    // passive bodies: controls and event commands are acknowledged/discarded
    // but never affect the trajectory. This prevents delayed commands from
    // firing when normal gameplay resumes.
    for (auto& [id, shipPtr] : m_simulation.ships())
    {
        Ship& ship = *shipPtr;

        auto controlIt = m_controlStreams.find(id.value);
        if (controlIt != m_controlStreams.end())
        {
            auto& stream = controlIt->second;

            if (time.universeTimeSimulation)
            {
                // Prediction is disabled on the client for this branch. Drain
                // every queued production input now so none can leak through
                // after the diagnostic branch is discarded.
                stream.discardPendingAndAcknowledgeNewest();
            }

            if (!time.universeTimeSimulation)
            {
                ShipControlState cmd;
                if (stream.consumeNext(cmd))
                {
                    // a destroyed ship takes no control (the tick is still acknowledged)
                    if (ship.core().transform().motion.destroyedCause != 0)
                    {
                        const auto tick = cmd.controlTick;
                        cmd = ShipControlState {};
                        cmd.controlTick = tick;
                    }
                    ship.setControlState(cmd);
                }
            }
        }

        /*
            Accelerated diagnostics consume/acknowledge incoming controls but
            never overwrite the frozen production control state. The branch is
            observational; leaving it resumes from the same gameplay state
            that existed before the diagnostic session.
        */
        auto cmdIt = m_pendingClientShipCommands.find(id.value);
        if (cmdIt == m_pendingClientShipCommands.end())
            continue;

        auto& cmdQueue = cmdIt->second;

        if (time.universeTimeSimulation || ship.core().transform().motion.destroyedCause != 0)
        {
            cmdQueue.clear();   // accelerated diagnostics / a destroyed ship: no commands
            continue;
        }

        while (!cmdQueue.empty())
        {
            const auto& shipCmd = cmdQueue.front();

            std::cout << "GameServer::update  - ClientShipCommand received: "
                    << shipCmd.type << "\n";

            switch (shipCmd.type)
            {
                case ClientShipCommand::EjectCockpitCapsule:
                    m_simulation.ejectShipCockpitCapsule(id);
                    break;

                case ClientShipCommand::StartBestRepairJob:
                    m_simulation.startBestRepairJobForFirstMissingSlot(id);
                    break;

                case ClientShipCommand::TogglePropulsionReactors:
                    ship.core().togglePropulsionReactors();
                    break;

                case ClientShipCommand::ToggleAssistedFreeRotation:
                    ship.core().toggleAssistedFreeRotation();
                    break;

                case ClientShipCommand::RequestDockDeparture:
                    if (shipCmd.index == 1)
                        m_simulation.releaseDockDepartureToPilot(id);
                    else
                        m_simulation.requestDockDeparture(id);
                    break;

                case ClientShipCommand::RequestDockArrival:
                    m_simulation.requestDockArrival(id, shipCmd.index != 0);
                    break;

                case ClientShipCommand::ToggleStabilization:
                    m_simulation.toggleStabilization(id);
                    break;

                case ClientShipCommand::ToggleJEngine:
                    m_simulation.toggleJEngine(id);
                    break;

                case ClientShipCommand::RequestJump:
                {
                    if (shipCmd.index < 0)
                    {
                        m_simulation.cancelJump(id, 8);
                        break;
                    }
                    const Ship* jumper = m_simulation.getShip(id);
                    const auto* from = jumper ? m_starAtlas.findSystemSummary(jumper->core().transform().motion.systemId) : nullptr;
                    const auto* to = m_starAtlas.findSystemSummary(shipCmd.index);
                    const double distanceLy = from && to ? glm::length(to->positionLy - from->positionLy) : 0.0;
                    // the exit body (unless the pilot named one): the planet with the most world sites,
                    // else the canon (observed / authored) planet, else the heaviest planet
                    std::string exitBody = shipCmd.detail;
                    // (an in-system jump names its body; without one the same system is refused)
                    if (exitBody.empty() && to && from != to)
                        if (const auto* system = m_starAtlas.findSystem(to->id))
                        {
                            std::map<std::string, int> sitesOnBody;
                            world::celestial::WorldSitesCatalog sites;
                            if (sites.loadFromRuntimeOrSource())
                                for (const auto& s : sites.sitesInSystem(to->id))
                                    ++sitesOnBody[s.parentBodyId];
                            std::tuple<int, int, double> best {-1, -1, -1.0};
                            for (const auto& b : system->bodies)
                            {
                                if (b.type != world::celestial::BodyType::Planet || b.radiusKm < 1000.0)
                                    continue;
                                const auto n = sitesOnBody.find(b.id);
                                const std::tuple<int, int, double> key {n == sitesOnBody.end() ? 0 : n->second, b.observed ? 1 : 0, b.massKg};
                                if (key > best)
                                {
                                    best = key;
                                    exitBody = b.id;
                                }
                            }
                        }
                    // an in-system jump is named after its body (the terminal shows "переход к <тело>")
                    std::string targetName = to ? to->name : std::string();
                    if (from && to && from == to && !exitBody.empty())
                        if (const auto* system = m_starAtlas.findSystem(to->id))
                            for (const auto& b : system->bodies)
                                if (b.id == exitBody && !b.name.empty())
                                    targetName = b.name;
                    // the interstellar step's line: toward the target star (the system frame shares the galactic axes)
                    glm::dvec3 dir = from && to ? glm::dvec3(to->positionLy - from->positionLy) : glm::dvec3(0.0);
                    double pathLy = distanceLy;
                    // the way starts at the ship's own point in an open-space place (a deep-space place, a system's approaches)
                    glm::dvec3 startLy = from ? from->positionLy : glm::dvec3(0.0);
                    if (from && jumper)
                        startLy = game::navigation::jump::jumpStartPointLy(m_starAtlas, from->id, jumper->core().transform().fullWorldMeters());
                    if (from && to && from != to && world::celestial::StarAtlasDatabase::isDeepSpace(from->id))
                    {
                        dir = to->positionLy - startLy;
                        pathLy = glm::length(to->positionLy - startLy);
                    }
                    if (from && to && from != to)
                    {
                        // around the charted masses on the line (InterstellarJumpRoute.h): the nose on the first step,
                        // the charge for the whole way; the steps after it are instant micro-exits of the same jump
                        auto route = game::navigation::jump::planInterstellarRoute(m_starAtlas, from->id, to->id, &startLy);
                        game::navigation::jump::applyLaneRecords(m_starAtlas, route, from->id, to->id,
                                                                 game::navigation::jump::parseLaneRecords(jumper && GameSimulation::navComputerWorks(*jumper) ? jumper->core().transform().motion.laneRecords : std::string()),
                                                                 m_universeClock.timeSeconds());
                        if (route.ok && route.pointsLy.size() > 2)
                        {
                            dir = route.pointsLy[1] - route.pointsLy[0];
                            pathLy = route.lengthLy;
                            std::string around;
                            for (const auto& n : route.avoidedNames)
                                around += (around.empty() ? "" : ", ") + n;
                            std::cerr << "[Jump] interstellar way to " << to->name << ": " << route.pointsLy.size() - 2
                                      << " correction(s) around " << around << ", " << distanceLy << " -> " << pathLy << " ly" << std::endl;
                        }
                        else if (!route.ok)
                            std::cerr << "[Jump] interstellar way to " << to->name << ": no way around found" << std::endl;
                    }
                    // the sumps between the systems on the way: the server's hidden field decides the catch now
                    GameSimulation::JumpDeepCatch deepCatch;
                    if (from && to && from != to && jumper)
                    {
                        namespace jj = game::navigation::jump;
                        auto route = jj::planInterstellarRoute(m_starAtlas, from->id, to->id, &startLy);
                        const double universeS = m_universeClock.timeSeconds();
                        for (const auto& use : jj::applyLaneRecords(m_starAtlas, route, from->id, to->id,
                                                                    jj::parseLaneRecords(GameSimulation::navComputerWorks(*jumper) ? jumper->core().transform().motion.laneRecords : std::string()),
                                                                    universeS))
                            std::cerr << "[Jump] lane " << use.shell->corridors[use.record.corridor - 1].name << " ("
                                      << (use.arrival ? "entry" : "exit") << ", " << use.record.source << ", "
                                      << (universeS - use.record.surveyUniverseS) / (365.25 * 86400.0) << " yr): "
                                      << jj::laneStateText(use.state) << ", offset "
                                      << jj::laneExpectedOffset(*use.shell, use.record, universeS) << " half-widths" << std::endl;
                        const double lock = jj::exitLockFactor(jj::systemHasBeacons(from->id), jj::systemHasBeacons(to->id));
                        std::mt19937_64 rng(static_cast<std::uint64_t>(id.value) * 0xA24BAED4963EE407ull +
                                            jumper->core().transform().motion.jumpSerial * 15485863ull + 7ull);
                        std::uniform_real_distribution<double> uniform(1.0e-12, 1.0);
                        double draw = -std::log(uniform(rng));
                        if (const char* dev = std::getenv("ELITE_DEV_SUMP_DRAW"); dev && *dev)
                            draw = std::max(1.0e-9, std::atof(dev));
                        for (const auto& c : jj::interstellarSumpCrossings(m_starAtlas, route, lock, universeS))
                            std::cerr << "[Jump] the way to " << to->name << " crosses " << (c.shell ? "the approach shell " : "the swarm ")
                                      << c.name << ": depth " << c.depth << " ly, catch " << 100.0 * c.probability << " % (lock " << lock
                                      << ")" << std::endl;
                        if (const auto c = jj::interstellarSumpCatch(m_starAtlas, route, lock, draw, universeS); c.caught)
                        {
                            deepCatch.placeId = c.placeId;
                            deepCatch.positionM = (c.pointLy - c.centreLy) * jj::MetersPerLy;
                            deepCatch.name = c.name;
                        }
                    }
                    m_simulation.requestJump(id, to ? to->id : -1, targetName, pathLy, exitBody, dir,
                                             deepCatch.placeId >= 0 ? &deepCatch : nullptr);
                    break;
                }

                case ClientShipCommand::NavData:
                    handleNavDataCommand(id, shipCmd.index, shipCmd.detail, static_cast<int>(shipCmd.amount));
                    break;

                case ClientShipCommand::SetFreeFlightThrust:
                    m_simulation.setFreeFlightThrust(id, shipCmd.index);
                    break;

                case ClientShipCommand::FireWeapon:
                    // 0 laser off, 1 laser on, 2 missile, 3 / 4 next laser / missile type (DEV),
                    // 5 DEV combat target `amount` m ahead
                    if (shipCmd.index == 0 || shipCmd.index == 1)
                        m_simulation.setLaserFiring(id, shipCmd.index == 1);
                    else if (shipCmd.index == 2)
                        m_simulation.launchMissile(id);
                    else if ((shipCmd.index == 3 || shipCmd.index == 4) && devCommandAccepted("weapon type", id))
                        m_simulation.cycleCombatWeapon(id, shipCmd.index == 4);
                    else if (shipCmd.index == 5 && devCommandAccepted("combat target", id))
                        m_simulation.spawnCombatTarget(id, shipCmd.amount > 0.0 ? shipCmd.amount : 400.0);
                    break;

                case ClientShipCommand::RouteService:
                    // the client flies routes (NAVIGATION_V2 §1); a stop's service stay is the server's
                    m_simulation.startHubService(id, shipCmd.target, shipCmd.detail);
                    break;

                case ClientShipCommand::RefitPreview:
                case ClientShipCommand::RefitApply:
                {
                    game::ship::refit::RefitOperation op;
                    op.kind = static_cast<game::ship::refit::RefitOperationKind>(
                        std::clamp(shipCmd.index, 0, static_cast<int>(game::ship::refit::RefitOperationKind::Install)));
                    op.moduleId = shipCmd.target;
                    op.productId = shipCmd.detail;
                    const EntityId subject {static_cast<std::uint32_t>(std::max(0.0, shipCmd.amount))};
                    if (shipCmd.type == ClientShipCommand::RefitPreview)
                        m_simulation.requestRefitPreview(id, subject, op);
                    else
                        m_simulation.applyRefit(id, subject, op);
                    break;
                }

                case ClientShipCommand::InjectEquipmentFailure:
                {
                    if (!devCommandAccepted("equipment failure", id))
                        break;
                    // A deliberate test fault on this player's authoritative
                    // installed hardware. The UI cannot pick another ship.
                    auto& core=ship.core();
                    const auto& desc=core.descriptor();
                    const auto& ids=desc.physicalControl.slotModuleIds;
                    const auto& slots=game::ship::plant::physicalControlSlots(
                        core.state().construction.physicalControlModel);
                    int slot=shipCmd.index;
                    const auto& runtime=static_cast<const
                        world::modules::ObjectModuleRuntime&>(
                            core.moduleRuntime());

                    // F is an on/off test fault, never a second permanent
                    // equipment failure. Revert precisely the internal fault
                    // this server previously injected on THIS ship. Genuine
                    // impact, wear, heat, emitter and structural damage stay.
                    const auto injected=m_injectedMainEngineFaults.find(id.value);
                    if(injected!=m_injectedMainEngineFaults.end())
                    {
                        const std::string restoreId=injected->second.moduleId;
                        const double previous=injected->second.previousInternalFailure01;
                        const bool restored=
                            core.moduleRuntime().debugRestoreInternalFailure(
                                restoreId,previous);
                        if(restored) core.refreshRuntimeState();
                        m_injectedMainEngineFaults.erase(injected);
                        const std::string report=
                            "[EquipmentFailure] module="+restoreId+
                            " status="+(restored?"RESTORED":"RESTORE_SKIPPED");
                        std::cerr<<report<<'\n';
#ifdef _WIN32
                        std::error_code error;
                        std::filesystem::create_directories("D:/tmp",error);
                        std::ofstream log("D:/tmp/elite_equipment_failures.log",
                            std::ios::app);
                        if(log) log<<report<<'\n';
#endif
                        break;
                    }
                    if(slot<0)
                    {
                        // First F selects one healthy installed main engine.
                        // The next F restores THAT same test fault before a
                        // different engine may be tested. No client-side ID.
                        for(std::size_t i=0;i<ids.size() &&
                            i<slots.size();++i)
                        {
                            if(slots[i].role==
                                game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                                continue;
                            const auto* mod=runtime.findModule(ids[i]);
                            if(mod && mod->state==
                                world::modules::ObjectModuleRuntimeState::Attached &&
                                mod->health>0.0f &&
                                !game::equipment::
                                    evaluateEquipmentFunctionAtDamage01(
                                        game::equipment::totalAccumulatedDamage01(
                                            mod->equipmentDamage.accumulated),
                                        mod->equipmentDamage.failure).failed)
                            {
                                slot=static_cast<int>(i);
                                break;
                            }
                        }
                    }
                    bool applied=false;
                    std::string moduleId;
                    if(core.state().actuators.source==
                           game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
                       slot>=0 && static_cast<std::size_t>(slot)<ids.size() &&
                       static_cast<std::size_t>(slot)<slots.size() &&
                       slots[slot].role!=
                           game::ship::plant::EngineSlotRole::ManoeuvreCluster &&
                       std::isfinite(shipCmd.amount) &&
                       shipCmd.amount>0.0 && shipCmd.amount<=1.0)
                    {
                        moduleId=ids[slot];
                        const auto definition=std::find_if(
                            desc.modules.begin(),desc.modules.end(),
                            [&](const auto& m){return m.moduleId==moduleId;});
                        const auto* mod=runtime.findModule(moduleId);
                        if(definition!=desc.modules.end() &&
                           definition->engine && mod &&
                           mod->state==
                               world::modules::ObjectModuleRuntimeState::Attached &&
                           mod->health>0.0f)
                        {
                            const double previousInternal=
                                mod->equipmentDamage.accumulated.internalFailure01;
                            applied=core.moduleRuntime().injectEquipmentFailure(
                                moduleId,shipCmd.amount);
                            if(applied)
                            {
                                m_injectedMainEngineFaults[id.value]={
                                    moduleId,previousInternal
                                };
                                core.refreshRuntimeState();
                            }
                        }
                    }
                    const std::string report=
                        std::string("[EquipmentFailure] active_model=")+
                        game::ship::plant::physicalControlModelName(
                            core.state().construction.physicalControlModel)+
                        " slot="+std::to_string(slot)+
                        " module="+moduleId+
                        " target="+std::to_string(shipCmd.amount)+
                        " status="+(applied?"APPLIED":"SKIPPED");
                    std::cerr<<report<<'\n';
#ifdef _WIN32
                    std::error_code error;
                    std::filesystem::create_directories("D:/tmp",error);
                    std::ofstream log(
                        "D:/tmp/elite_equipment_failures.log",std::ios::app);
                    if(log)
                        log<<report<<'\n';
#endif
                    break;
                }

                case ClientShipCommand::SelectPhysicalControlModel:
                {
                    using Model = game::ship::plant::PhysicalControlModelId;
                    const int raw = shipCmd.index;

                    if (raw >= static_cast<int>(Model::AssistedI) &&
                        raw <= static_cast<int>(Model::NewtonianX))
                    {
                        (void)ship.core().selectPhysicalConstructionForDiagnostic(
                            static_cast<Model>(raw)
                        );
                    }
                    break;
                }

                default:
                    ship.applyCommand(shipCmd);
                    break;
            }

            cmdQueue.pop_front();
        }
    }



    // Player movement is fully driven by the ordinary numbered
    // ShipControlState stream. Navigation/autopilot lives on the client and
    // is indistinguishable here from human control input.

m_simulation.setOrbitalUniverseTimeSeconds(
    universeTime
);

// Multi-system tick (author 2026-11-08: "сервер должен считать те системы или участки систем где
// находятся игроки", "подключишь симуляцию всех систем"): every system with a ship in it gets a
// pass - its bodies from the analytic ephemeris at this universe time (nothing is integrated for
// them), its gravity / hubs as the active context - and the simulation advances that system's
// ships. Pass 0 also runs the once-per-tick work. The system the server started in is always
// simulated (its hubs / dock traffic run without ships).
{
    const auto passStart = std::chrono::steady_clock::now();
    std::vector<int> passSystems;
    if (m_initialSimulationSystemId >= 0)
        passSystems.push_back(m_initialSimulationSystemId);
    for (const auto& [shipId, shipPtr] : m_simulation.ships())
    {
        (void)shipId;
        if (!shipPtr)
            continue;
        const int systemId = shipPtr->core().transform().motion.systemId;
        if (systemId >= 0 && std::find(passSystems.begin(), passSystems.end(), systemId) == passSystems.end())
            passSystems.push_back(systemId);
    }
    for (std::size_t pass = 0; pass < passSystems.size(); ++pass)
    {
        switchSimulationContext(passSystems[pass]);
        m_simulation.setSystemPass(static_cast<int>(pass));
        m_simulation.update(time);
    }
    m_simulation.setSystemPass(0);
    m_lastTickSystemPasses = passSystems.size();
    m_lastTickSimulationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - passStart).count();
    game::server::serverTickProfile().simulation += m_lastTickSimulationSeconds;
}

// Navigation sensors observe the completed authoritative epoch. Their cadence
// is device-owned and uses universe time; render/snapshot FPS cannot create a
// measurement. The synthetic radar asks for truth rows only when a scan is due.
{
    game::server::ScopedTickTimer timer(game::server::serverTickProfile().sensors);
    updateNavigationSensorDevices(universeTime);
}
// DEV / load test (author 2026-11-08: "погоняй эту симуляцию ... с тиками по экономике"): the
// economy is a deterministic function of the day computed by the clients today; with
// ELITE_DEV_ECONOMY_TICK_S = T the server recomputes every hub's market / events / board once per
// T game seconds (each round a new day) - the work a server-held economy would do per day. The
// round is spread over the T seconds, a slice of hubs per tick (all at once: 57 ms, a 170 ms tick).
{
    static const double economyTickS = [] {
        const char* v = std::getenv("ELITE_DEV_ECONOMY_TICK_S");
        return v && *v ? std::atof(v) : 0.0;
    }();
    static std::size_t economyNext = 0;
    static int economyDayOffset = 0;
    const auto& economy = game::economy::EconomyModel::instance();
    if (economyTickS > 0.0 && !economy.hubs().empty() && time.gameplayDeltaSeconds > 0.0)
    {
        auto& profile = game::server::serverTickProfile();
        game::server::ScopedTickTimer timer(profile.economy);
        const auto& hubs = economy.hubs();
        const double ticksPerRound = std::max(1.0, economyTickS / time.gameplayDeltaSeconds);
        const std::size_t slice = static_cast<std::size_t>(std::ceil(static_cast<double>(hubs.size()) / ticksPerRound));
        const int day = game::economy::EconomyModel::dayOf(universeTime) + economyDayOffset;
        for (std::size_t k = 0; k < slice && economyNext < hubs.size(); ++k, ++economyNext)
        {
            const auto& hub = hubs[economyNext];
            const auto quotes = economy.market(hub.id, day);
            const auto events = economy.events(hub.id, day);
            const auto board = economy.board(hub.id, day);
            profile.economyQuotes += static_cast<long long>(quotes.size() + events.size() + board.size());
        }
        if (economyNext >= hubs.size())
        {
            economyNext = 0;
            ++economyDayOffset;
            ++profile.economyTicks;
        }
    }
}











    if (m_forceSnapshotPublication ||
        m_serverTick % m_snapshotInterval == 0)
    {
        // Snapshot construction belongs to the publication cadence. The
        // simulation step above mutates authoritative state only; replication
        // DTOs are materialized here when they can actually be delivered.
        game::server::ScopedTickTimer timer(game::server::serverTickProfile().snapshotBuild);
        {
            game::server::ScopedTickTimer t1(game::server::serverTickProfile().snapshotParts[0]);
            m_lastSnapshot =
                m_simulation.buildReplicationSnapshot(m_serverTick);
        }
        {
            game::server::ScopedTickTimer t2(game::server::serverTickProfile().snapshotParts[1]);
            populateClientSessionSnapshot(m_lastSnapshot);
        }
        game::server::ScopedTickTimer t3(game::server::serverTickProfile().snapshotParts[2]);
        m_canonicalReplicationSnapshot =
            m_lastSnapshot.replication.entitySetMode == game::network::ReplicatedEntitySetMode::FullAuthoritativeSet
            ? game::network::materializeCanonicalFromFullSet(std::move(m_canonicalReplicationSnapshot), m_lastSnapshot)
            : game::network::materializeCanonicalReplicationSnapshot(
                &m_canonicalReplicationSnapshot,
                m_lastSnapshot
            );
        m_forceSnapshotPublication = false;
    }

    {
        game::server::ScopedTickTimer timer(game::server::serverTickProfile().mapRequests);
        processPendingMapRequests();
    }
}

bool GameServer::enqueueMapRequest(
    game::network::ServerSessionId sessionId,
    const game::network::MapRequest& request
)
{
    if (controlledEntityForSession(sessionId).value == 0)
    {
        ++m_queueDiagnostics.rejectedSessionMessages;
        return false;
    }

    if (m_pendingMapRequests.size() >= MaxPendingMapRequests)
    {
        m_pendingMapRequests.pop_front();
        ++m_queueDiagnostics.droppedMapRequests;
    }

    PendingSessionMapRequest pending;
    pending.sessionId = sessionId;
    pending.request = request;
    m_pendingMapRequests.push_back(std::move(pending));
    return true;
}

bool GameServer::popMapResponse(
    game::network::ServerSessionId& outSessionId,
    game::network::MapResponse& outResponse
)
{
    if (m_completedMapResponses.empty())
        return false;

    auto completed = std::move(m_completedMapResponses.front());
    m_completedMapResponses.pop_front();

    outSessionId = completed.sessionId;
    outResponse = std::move(completed.response);
    return true;
}

void GameServer::queueMapResponse(
    game::network::ServerSessionId sessionId,
    game::network::MapResponse response
)
{
    if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
    {
        m_completedMapResponses.pop_front();
        ++m_queueDiagnostics.droppedMapResponses;
    }

    CompletedSessionMapResponse completed;
    completed.sessionId = sessionId;
    completed.response = std::move(response);
    m_completedMapResponses.push_back(std::move(completed));
}

void GameServer::processPendingMapRequests()
{
    const auto metadata = protocolMetadata();
    while (!m_pendingMapRequests.empty())
    {
        auto pending = std::move(m_pendingMapRequests.front());
        m_pendingMapRequests.pop_front();

        const auto sessionId = pending.sessionId;

        // A disconnect after enqueue but before execution must not leak a map
        // response to a dead/reused transport binding.
        if (controlledEntityForSession(sessionId).value == 0)
            continue;

        std::visit(
            [this, &metadata, sessionId](const auto& typedRequest)
            {
                using RequestT = std::decay_t<decltype(typedRequest)>;

                static_assert(
                    std::is_same_v<RequestT, game::network::GalaxyMapRequest>,
                    "MapRequest must remain Galaxy-only until another map RPC carries unique authoritative data"
                );

                game::network::GalaxyMapResponse response;
                response.requestId = typedRequest.requestId;
                response.metadata = metadata;
                response.snapshot = buildGalaxyMapSnapshot();
                queueMapResponse(sessionId, std::move(response));
            },
            pending.request
        );
    }
}

void GameServer::populateClientSessionSnapshot(
    SimulationSnapshot& snapshot
) const
{
    snapshot.metadata.serverTick = m_serverTick;
    snapshot.metadata.serverTimeSeconds = m_simulation.serverTime();
    snapshot.metadata.universeTimeSeconds = m_universeClock.timeSeconds();
    snapshot.metadata.universeTimelineRevision =
        m_universeTimelineRevision;

    for (auto& ship : snapshot.ships)
    {
        const auto it = m_controlStreams.find(ship.id.value);
        ship.acknowledgedControlTick =
            it != m_controlStreams.end()
                ? it->second.lastProcessedTick()
                : 0;
    }

    // Shared publication state has no player/session navigation identity.
    // ServerRunner must compose that field for the destination session.
    snapshot.session.playerNavigation = {};
    snapshot.session.ownedNavigationAssets.clear();
    snapshot.session.navigationSensors = {};
    snapshot.session.predictionWorldParams = m_simulation.world();
    snapshot.session.universeTimeSeconds =
        m_universeClock.timeSeconds();
    snapshot.session.universeTimeScale =
        m_universeClock.timeScale();
    snapshot.session.universeTimelineRevision =
        m_universeTimelineRevision;
    snapshot.session.configuredUniverseTimeScale =
        m_universeClock.configuredTimeScale();
    snapshot.session.universeTimeSimulation =
        m_universeClock.simulationMode();
    snapshot.session.universeDate =
        m_universeClock.dateTimeString();
}



void GameServer::submitCommand(EntityId id, const ShipControlState& control)
{
    auto& stream = m_controlStreams[id.value];

    const auto result = stream.enqueue(control);

    using EnqueueResult =
        game::server::FixedStepControlQueue::EnqueueResult;

    if (result == EnqueueResult::Stale)
    {
        ++m_queueDiagnostics.staleControlCommands;
        return;
    }

}

void GameServer::resetSessionControlState(
    EntityId controlledEntityId,
    const char* reason
)
{
    if (controlledEntityId.value == 0)
        return;

    std::uint64_t previousLastReceived = 0;
    std::uint64_t previousLastProcessed = 0;
    std::size_t previousPendingControls = 0;

    const auto streamIt = m_controlStreams.find(controlledEntityId.value);
    if (streamIt != m_controlStreams.end())
    {
        previousLastReceived = streamIt->second.lastReceivedTick();
        previousLastProcessed = streamIt->second.lastProcessedTick();
        previousPendingControls = streamIt->second.pendingCount();
        m_controlStreams.erase(streamIt);
    }

    std::size_t previousPendingCommands = 0;
    const auto commandIt =
        m_pendingClientShipCommands.find(controlledEntityId.value);
    if (commandIt != m_pendingClientShipCommands.end())
    {
        previousPendingCommands = commandIt->second.size();
        m_pendingClientShipCommands.erase(commandIt);
    }

    // Continuous control belongs to the live session too. Do not leave the
    // ship accelerating or rotating from the final sample of a disconnected
    // client. Newtonian velocity/orientation remain authoritative state; only
    // the pilot intent is neutralized.
    if (Ship* ship = m_simulation.getShip(controlledEntityId))
        ship->setControlState(ShipControlState{});

    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-CONTROL][server] stream-reset entity="
            << controlledEntityId.value
            << " reason=" << (reason ? reason : "session-boundary")
            << " previous_last_received=" << previousLastReceived
            << " previous_last_processed=" << previousLastProcessed
            << " pending_controls=" << previousPendingControls
            << " pending_commands=" << previousPendingCommands
            << "\n";
}



game::network::ServerSessionId GameServer::createPlayerSession(
    PlayerId playerId
)
{
    const PlayerState* player = m_players.find(playerId);
    if (!player)
        return {};

    const EntityId controlledEntityId =
        m_controls.controlledEntity(playerId);

    if (controlledEntityId.value == 0 ||
        !m_simulation.getShip(controlledEntityId))
    {
        return {};
    }

    const ShipInstanceId controlledShipInstanceId =
        m_shipInstances.instanceForEntity(controlledEntityId);

    if (controlledShipInstanceId == 0 ||
        controlledShipInstanceId != player->currentShipId)
    {
        return {};
    }

    const auto sessionId = m_sessions.create(playerId);
    if (!sessionId)
        return {};

    // controlTick is a sequence inside one live client session, not a
    // persistent property of the materialized ship. A new GameClient starts
    // at tick 1, so an EntityId-keyed stream from an older session must never
    // reject the new epoch as stale. This also scrubs any one-shot commands
    // left behind by an interrupted connection.
    resetSessionControlState(controlledEntityId, "session-create");

    m_simulation.setPlayerControlled(controlledEntityId, true);

    if (Ship* ship = m_simulation.getShip(controlledEntityId))
    {
        const auto& radar = ship->core().radar();
        const auto& desc = radar.getDesc();
        if (desc.backendKind == game::RadarBackendKind::TestIdeal)
        {
            auto& runtime = m_playerRadarRuntimes[controlledShipInstanceId];
            if (!runtime.configured)
            {
                runtime.unit.configure(desc, controlledShipInstanceId);
                runtime.configured = true;
            }

            runtime.radarOperational =
                radar.isOperational() &&
                radar.getAvailablePower() + 1.0e-9 >= desc.powerConsumption;
            const auto& transform = ship->core().transform();
            runtime.navigationSolution = runtime.unit.navigationSolution(
                m_universeClock.timeSeconds(),
                transform.worldPosition,
                transform.motion.worldVelocityMps
            );
            runtime.hasNavigationSolution = true;
        }
    }

    return sessionId;
}

bool GameServer::disconnectPlayerSession(
    game::network::ServerSessionId sessionId
)
{
    const PlayerId playerId = m_sessions.player(sessionId);
    const EntityId controlledEntityId =
        m_controls.controlledEntity(playerId);

    if (!playerId ||
        controlledEntityId.value == 0 ||
        !m_sessions.disconnect(sessionId))
    {
        return false;
    }

    // Persistent player->ship control identity survives a disconnect, but
    // transport input does not. Once the last live session for this player is
    // gone, discard its numbered-input epoch and neutralize continuous pilot
    // intent before releasing the expensive Active/player-controlled pin.
    if (!m_sessions.isConnectedPlayer(playerId))
    {
        resetSessionControlState(controlledEntityId, "session-disconnect");
        m_simulation.setPlayerControlled(controlledEntityId, false);
    }

    return true;
}

PlayerId GameServer::playerForSession(
    game::network::ServerSessionId sessionId
) const noexcept
{
    return m_sessions.player(sessionId);
}

EntityId GameServer::controlledEntityForSession(
    game::network::ServerSessionId sessionId
) const noexcept
{
    return m_controls.controlledEntity(m_sessions.player(sessionId));
}

ShipInstanceId GameServer::controlledShipInstanceForSession(
    game::network::ServerSessionId sessionId
) const noexcept
{
    return m_shipInstances.instanceForEntity(
        controlledEntityForSession(sessionId)
    );
}

std::size_t GameServer::connectedPlayerSessionCount() const noexcept
{
    return m_sessions.connectedCount();
}

std::vector<PlayerId> GameServer::playerIdentities() const
{
    std::vector<PlayerId> out;
    out.reserve(m_players.size());

    for (const auto& [rawPlayerId, player] : m_players.all())
    {
        (void)player;
        const PlayerId id {rawPlayerId};
        if (id)
            out.push_back(id);
    }

    std::sort(
        out.begin(),
        out.end(),
        [](PlayerId a, PlayerId b)
        {
            return a.value < b.value;
        }
    );
    return out;
}

void GameServer::receiveClientMessage(
    game::network::ServerSessionId sessionId,
    const game::network::ClientMessage& msg)
{
    const PlayerId playerId = m_sessions.player(sessionId);
    const EntityId controlledEntityId =
        controlledEntityForSession(sessionId);

    if (!playerId || controlledEntityId.value == 0)
    {
        ++m_queueDiagnostics.rejectedSessionMessages;
        return;
    }

    std::visit(
        [this, controlledEntityId](const auto& payload)
        {
            using PayloadT = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<PayloadT, ShipControlState>)
            {
                // Server sees only ordinary ship controls. It deliberately
                // does not know whether Human or a client virtual pilot
                // produced this numbered input sample.
                submitCommand(controlledEntityId, payload);
            }
            else if constexpr (
                std::is_same_v<
                    PayloadT,
                    game::network::ClientObservationInterest>)
            {
                std::vector<
                    game::simulation::ShipClientObservationInterestEntry
                > entries;
                entries.reserve(payload.entries.size());

                for (const auto& entry : payload.entries)
                {
                    entries.push_back({
                        entry.targetId,
                        entry.detail ==
                                game::network::ClientObservationDetail::Response
                            ? game::simulation::ShipMotionFidelity::Response
                            : game::simulation::ShipMotionFidelity::Kinematic
                    });
                }

                (void)m_simulation.submitClientObservationInterest(
                    controlledEntityId,
                    payload.serial,
                    entries
                );
            }
            else if constexpr (std::is_same_v<PayloadT, ClientShipCommand>)
            {
                // Non-navigation service/debug commands remain explicit.
                auto& queue =
                    m_pendingClientShipCommands[controlledEntityId.value];
                if (queue.size() >= MaxShipCommandsPerShip)
                {
                    ++m_queueDiagnostics.droppedShipCommands;
                    return;
                }
                queue.push_back(payload);
            }
        },
        msg.payload
    );
}


void GameServer::debugRefreshSnapshot()
{
    // Debug panels are allowed to request heavy structural data.
    // This keeps regular published snapshots lightweight, while
    // structure_debug.html still receives modules/links on demand.
    m_simulation.debugForceFullShipGraphPayload();

    // The full graph is published by the next normal authoritative tick.
    // Debug UI must never run GameSimulation::update() out of band.
    m_forceSnapshotPublication = true;
}


void GameServer::updateNavigationSensorDevices(double universeTimeSeconds)
{
    constexpr std::uint64_t ShipTruthKeyPrefix = 0x1000000000000000ULL;
    constexpr std::uint64_t StaticTruthKeyPrefix = 0x2000000000000000ULL;
    constexpr std::uint64_t TruthKeyPayloadMask = 0x0FFFFFFFFFFFFFFFULL;

    for (const EntityId observerId : m_simulation.playerControlledShipIds())
    {
        Ship* observer = m_simulation.getShip(observerId);
        if (!observer)
            continue;

        const ShipInstanceId observerInstanceId =
            m_shipInstances.instanceForEntity(observerId);
        if (observerInstanceId == 0)
            continue;

        const auto& installedRadar = observer->core().radar();
        const auto& desc = installedRadar.getDesc();
        if (desc.backendKind != game::RadarBackendKind::TestIdeal)
            continue;

        auto& runtime = m_playerRadarRuntimes[observerInstanceId];
        if (!runtime.configured)
        {
            runtime.unit.configure(desc, observerInstanceId);
            runtime.configured = true;
        }

        const bool powered =
            installedRadar.isOperational() &&
            installedRadar.getAvailablePower() + 1.0e-9 >=
                desc.powerConsumption;
        runtime.radarOperational = powered;

        const auto& observerTransform = observer->core().transform();
        runtime.navigationSolution = runtime.unit.navigationSolution(
            universeTimeSeconds,
            observerTransform.worldPosition,
            observerTransform.motion.worldVelocityMps
        );
        runtime.hasNavigationSolution = true;

        if (powered && runtime.unit.measurementDue(universeTimeSeconds))
        {
            std::vector<game::radar::TestIdealRadarTruthContact> truth;
            truth.reserve(
                m_simulation.ships().size() +
                m_simulation.staticObjects().size()
            );

            const int observerSystemId = observerTransform.motion.systemId;

            for (const auto& [targetId, targetPtr] : m_simulation.ships())
            {
                if (!targetPtr || targetId == observerId)
                    continue;

                const auto& targetTransform = targetPtr->core().transform();
                if (targetTransform.motion.systemId != observerSystemId)
                    continue;

                const ShipInstanceId targetInstanceId =
                    m_shipInstances.instanceForEntity(targetId);
                const std::uint64_t stableTargetKey =
                    targetInstanceId != 0
                        ? static_cast<std::uint64_t>(targetInstanceId)
                        : static_cast<std::uint64_t>(targetId.value);

                game::radar::TestIdealRadarTruthContact row;
                row.sourceKey = ShipTruthKeyPrefix |
                    (stableTargetKey & TruthKeyPayloadMask);
                row.worldPosition = targetTransform.worldPosition;
                row.worldVelocityMps =
                    targetTransform.motion.worldVelocityMps;
                row.radarCrossSection =
                    targetPtr->core().desc().radarCrossSection;
                row.kind = game::radar::RadarTrackKindShip;
                truth.push_back(row);
            }

            // Compact traffic shells (hub dock traffic, route traffic) are
            // ships too: without them the hub traffic is invisible.
            for (const auto& [shellId, shell] : m_simulation.shipShells())
            {
                if (!shell.valid() ||
                    shell.transform.motion.systemId != observerSystemId)
                {
                    continue;
                }

                game::radar::TestIdealRadarTruthContact row;
                row.sourceKey = ShipTruthKeyPrefix |
                    (static_cast<std::uint64_t>(shellId.value) &
                     TruthKeyPayloadMask);
                row.worldPosition = shell.transform.worldPosition;
                row.worldVelocityMps = shell.transform.motion.worldVelocityMps;
                row.radarCrossSection = shell.definition->radarCrossSection;
                row.kind = game::radar::RadarTrackKindShip;
                truth.push_back(row);
            }

            for (const auto& [targetId, object] : m_simulation.staticObjects())
            {
                if (object.systemId != observerSystemId)
                    continue;

                game::radar::TestIdealRadarTruthContact row;
                row.sourceKey = StaticTruthKeyPrefix |
                    (static_cast<std::uint64_t>(targetId.value) &
                     TruthKeyPayloadMask);
                row.worldPosition = object.worldPosition;
                row.worldVelocityMps = glm::dvec3(object.linearVelocity);
                row.radarCrossSection = 1.0;
                row.kind = game::radar::RadarTrackKindStructure;
                truth.push_back(row);
            }

            runtime.unit.captureMeasurement(
                universeTimeSeconds,
                observerTransform.worldPosition,
                observerTransform.motion.worldVelocityMps,
                observerTransform.orientation,
                truth
            );
        }

        // Processing latency is independent from measurement cadence. A report
        // becomes visible only after availableAt even if no new scan is due.
        runtime.unit.advanceAvailability(universeTimeSeconds);
    }
}


game::simulation::ClientNavigationSensorSnapshot
GameServer::navigationSensorsForSession(
    game::network::ServerSessionId sessionId
) const
{
    game::simulation::ClientNavigationSensorSnapshot out;

    const EntityId controlledEntityId =
        controlledEntityForSession(sessionId);
    if (controlledEntityId.value == 0)
        return out;

    const Ship* ship = m_simulation.getShip(controlledEntityId);
    if (!ship)
        return out;

    const auto& installedRadar = ship->core().radar();
    const auto& desc = installedRadar.getDesc();
    out.radarInstalled =
        desc.backendKind == game::RadarBackendKind::TestIdeal;

    const ShipInstanceId shipInstanceId =
        m_shipInstances.instanceForEntity(controlledEntityId);
    const auto runtimeIt = m_playerRadarRuntimes.find(shipInstanceId);
    if (runtimeIt == m_playerRadarRuntimes.end() ||
        !runtimeIt->second.configured)
    {
        return out;
    }

    const auto& runtime = runtimeIt->second;
    out.radarOperational = out.radarInstalled && runtime.radarOperational;

    const auto& unit = runtime.unit;
    if (unit.hasAvailableScan())
    {
        out.hasRadarScan = true;
        out.latestRadarScan = unit.latestAvailableScan();
    }

    if (runtime.hasNavigationSolution)
    {
        out.hasNavigationSolution = true;
        out.navigationSolution = runtime.navigationSolution;
    }
    return out;
}


std::vector<game::navigation::OwnedNavigationAsset>
GameServer::ownedNavigationAssetsForSession(
    game::network::ServerSessionId sessionId
) const
{
    std::vector<game::navigation::OwnedNavigationAsset> out;
    const PlayerId playerId = m_sessions.player(sessionId);
    if (!playerId)
        return out;

    for (const auto& [shipInstanceId, ownership] : m_shipOwnership.all())
    {
        if (ownership.owner.kind != game::server::ShipOwnerKind::Player ||
            ownership.owner.playerId != playerId)
        {
            continue;
        }

        const auto* instance = m_shipInstances.find(shipInstanceId);
        if (!instance)
            continue;

        game::navigation::OwnedNavigationAsset asset;
        asset.asset = game::navigation::NavigationAssetRef::ship(shipInstanceId);
        asset.materializedEntityId = instance->materializedEntityId;
        asset.typeId = instance->typeId;
        asset.displayName = instance->name;

        // Direct personal ownership is the command policy for this milestone.
        // Fleet/organization delegation must be added as an explicit authority
        // policy later; knowing an instance ID is never enough by itself.
        asset.commandable = true;

        if (instance->materialized())
        {
            if (const Ship* ship = m_simulation.getShip(instance->materializedEntityId))
            {
                const auto& transform = ship->core().transform();
                asset.worldPosition = transform.worldPosition;
                asset.worldVelocityMps = transform.motion.worldVelocityMps;
                asset.kinematicsValid = true;
            }
        }

        out.push_back(std::move(asset));
    }

    std::sort(
        out.begin(),
        out.end(),
        [](const game::navigation::OwnedNavigationAsset& a,
           const game::navigation::OwnedNavigationAsset& b)
        {
            return a.asset.shipInstanceId < b.asset.shipInstanceId;
        }
    );
    return out;
}

const SimulationSnapshot& GameServer::snapshot() const
{
    return m_lastSnapshot;
}

bool GameServer::navigationStateForSession(
    game::network::ServerSessionId sessionId,
    world::celestial::PlayerNavigationState& outNavigation
) const
{
    const EntityId controlledEntityId =
        controlledEntityForSession(sessionId);

    if (controlledEntityId.value == 0)
        return false;

    outNavigation = navigationStateForEntity(controlledEntityId);
    return true;
}

bool GameServer::copySnapshotForSession(
    game::network::ServerSessionId sessionId,
    SimulationSnapshot& outSnapshot
) const
{
    world::celestial::PlayerNavigationState sessionNavigation;
    if (!navigationStateForSession(sessionId, sessionNavigation))
        return false;

    outSnapshot = m_lastSnapshot;
    outSnapshot.session.playerNavigation = sessionNavigation;
    outSnapshot.session.ownedNavigationAssets =
        ownedNavigationAssetsForSession(sessionId);
    outSnapshot.session.navigationSensors =
        navigationSensorsForSession(sessionId);

    // Full copy remains available for diagnostics/contracts. Production normal
    // publication switches to copySparseSnapshotForSession in Stage M7; initial
    // connection bootstrap uses copyHydratedSnapshotForSession.
    outSnapshot.replication.entitySetMode =
        game::network::ReplicatedEntitySetMode::FullAuthoritativeSet;
    outSnapshot.replication.removedShipIds.clear();
    outSnapshot.replication.removedObjectIds.clear();
    outSnapshot.replication.removedHubIds.clear();
    return true;
}


bool GameServer::copyHydratedSnapshotForSession(
    game::network::ServerSessionId sessionId,
    SimulationSnapshot& outSnapshot
) const
{
    world::celestial::PlayerNavigationState sessionNavigation;
    if (!navigationStateForSession(sessionId, sessionNavigation))
        return false;

    // Late join must not depend on whether this particular publication happened
    // to carry a dirty structural graph. The canonical source retains the most
    // recent authoritative value for every sparse nested graph field.
    outSnapshot = m_canonicalReplicationSnapshot;
    outSnapshot.session.playerNavigation = sessionNavigation;
    outSnapshot.session.ownedNavigationAssets =
        ownedNavigationAssetsForSession(sessionId);
    outSnapshot.session.navigationSensors =
        navigationSensorsForSession(sessionId);
    outSnapshot.replication.entitySetMode =
        game::network::ReplicatedEntitySetMode::FullAuthoritativeSet;
    outSnapshot.replication.removedShipIds.clear();
    outSnapshot.replication.removedObjectIds.clear();
    outSnapshot.replication.removedHubIds.clear();
    return true;
}

bool GameServer::copySparseSnapshotForSession(
    game::network::ServerSessionId sessionId,
    const game::server::ReplicationPublicationSelection& selection,
    SimulationSnapshot& outSnapshot,
    int& inOutPublishedSystemId
) const
{
    world::celestial::PlayerNavigationState sessionNavigation;
    if (!navigationStateForSession(sessionId, sessionNavigation))
        return false;

    // Multi-system server (load test 2026-11-08: every snapshot carried all 110 station objects /
    // 107 hubs / all signals of all 61 systems, 134 KB, 2.2 MB/s per player): a session gets the
    // objects / hubs / signals of its own system only; the ships are selected below (interest).
    int systemId = -1;
    {
        const EntityId ownId = controlledEntityForSession(sessionId);
        bool found = false;
        for (const auto& ship : m_lastSnapshot.ships)
            if (ship.id == ownId)
            {
                systemId = ship.transform.motion.systemId;
                found = true;
                break;
            }
        if (!found)
            if (const Ship* own = m_simulation.getShip(ownId))
                systemId = own->core().transform().motion.systemId;
    }
    outSnapshot.metadata = m_lastSnapshot.metadata;
    outSnapshot.replication = m_lastSnapshot.replication;
    outSnapshot.session = m_lastSnapshot.session;
    outSnapshot.signals.clear();
    for (const auto& signal : m_lastSnapshot.signals)
        if (signal.systemId == systemId)
            outSnapshot.signals.push_back(signal);
    outSnapshot.objects.clear();
    for (const auto& object : m_lastSnapshot.objects)
        if (object.systemId == systemId)
            outSnapshot.objects.push_back(object);
    outSnapshot.hubs.clear();
    for (const auto& hub : m_lastSnapshot.hubs)
        if (hub.systemId == systemId)
            outSnapshot.hubs.push_back(hub);
    const int leftSystemId = inOutPublishedSystemId != systemId ? inOutPublishedSystemId : -1;
    inOutPublishedSystemId = systemId;
    outSnapshot.session.playerNavigation = sessionNavigation;
    outSnapshot.session.ownedNavigationAssets =
        ownedNavigationAssetsForSession(sessionId);
    outSnapshot.session.navigationSensors =
        navigationSensorsForSession(sessionId);
    outSnapshot.replication.entitySetMode =
        game::network::ReplicatedEntitySetMode::SparseRetainMissing;
    outSnapshot.replication.removedShipIds = selection.removedShipIds;
    outSnapshot.replication.removedObjectIds = selection.removedObjectIds;
    outSnapshot.replication.removedHubIds = selection.removedHubIds;
    if (leftSystemId >= 0)
    {
        // the session left a system (a jump): the client drops what it kept of it
        for (const auto& object : m_lastSnapshot.objects)
            if (object.systemId == leftSystemId)
                outSnapshot.replication.removedObjectIds.push_back(object.id);
        for (const auto& hub : m_lastSnapshot.hubs)
            if (hub.systemId == leftSystemId)
                outSnapshot.replication.removedHubIds.push_back(hub.id);
    }

    const auto isHydrationId =
        [&](EntityId id)
        {
            return std::find(
                selection.shipHydrationIds.begin(),
                selection.shipHydrationIds.end(),
                id
            ) != selection.shipHydrationIds.end();
        };

    std::vector<ShipSnapshot> selectedShips;
    selectedShips.reserve(selection.shipUpdateIds.size());

    for (const EntityId id : selection.shipUpdateIds)
    {
        const auto& source = isHydrationId(id)
            ? m_canonicalReplicationSnapshot.ships
            : m_lastSnapshot.ships;

        const auto it = std::find_if(
            source.begin(),
            source.end(),
            [&](const ShipSnapshot& ship)
            {
                return ship.id == id;
            }
        );

        // A lifecycle removal can race a selection only across programmer
        // error here because both are derived from one immutable publication.
        // Fail closed by omitting the missing row; explicit removal is already
        // carried separately when the entity left the source set.
        if (it != source.end())
            selectedShips.push_back(*it);
    }

    outSnapshot.ships = std::move(selectedShips);

    // Stage M7 decimates ship payload only. Objects/hubs/signals retain their
    // existing publication cadence, while explicit object/hub removal rows keep
    // lifecycle semantics correct under the sparse entity-set envelope.
    return true;
}

game::server::ShipReplicationInterestPlan
GameServer::shipReplicationInterestPlanForSession(
    game::network::ServerSessionId sessionId
) const
{
    const EntityId controlledEntityId =
        controlledEntityForSession(sessionId);

    if (controlledEntityId.value == 0)
        return {};

    return game::server::buildShipReplicationInterestPlan(
        controlledEntityId,
        m_lastSnapshot,
        m_replicationInterestPolicy
    );
}

EntityId GameServer::playerId() const
{
    return m_simulation.playerId();
}

WorldParams& GameServer::world()
{
    return m_simulation.world();
}

bool GameServer::debugDestroyShipModule(EntityId shipId, const std::string& moduleId)
{
    const bool ok = m_simulation.debugDestroyShipModule(shipId, moduleId);

    std::cout
        << "[GameServer] debugDestroyShipModule entityId="
        << shipId.value
        << " moduleId=" << moduleId
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}


bool GameServer::debugSetShipStructuralLinkHealth(
    EntityId id,
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    return m_simulation.debugSetShipStructuralLinkHealth(
        id,
        linkId,
        health,
        destroyed
    );
}


bool GameServer::debugDetachShipModule(EntityId id, const std::string& moduleId)
{
    return m_simulation.debugDetachShipModule(id, moduleId);
}


bool GameServer::debugReattachShipModule(
    EntityId id,
    const std::string& moduleId
)
{
    const bool ok = m_simulation.debugReattachShipModule(id, moduleId);

    std::cout
        << "[GameServer] debugReattachShipModule entityId="
        << id.value
        << " moduleId=" << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}



bool GameServer::startShipRepairJob(
    EntityId id,
    const std::string& moduleId
)
{
    const bool ok =
        m_simulation.startShipRepairJob(id, moduleId);

    std::cout
        << "[GameServer] startShipRepairJob entityId="
        << id.value
        << " moduleId=" << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}


bool GameServer::startBestRepairJobForMissingSlot(
    EntityId targetShipId,
    const std::string& targetModuleId
)
{
    const bool ok =
        m_simulation.startBestRepairJobForMissingSlot(
            targetShipId,
            targetModuleId
        );

    std::cout
        << "[GameServer] startBestRepairJobForMissingSlot shipId="
        << targetShipId.value
        << " targetModuleId="
        << targetModuleId
        << " ok="
        << ok
        << "\n";

    return ok;
}


bool GameServer::startBestRepairJobForFirstMissingSlot(EntityId targetShipId)
{
    const bool ok =
        m_simulation.startBestRepairJobForFirstMissingSlot(targetShipId);

    std::cout
        << "[GameServer] startBestRepairJobForFirstMissingSlot shipId="
        << targetShipId.value
        << " ok=" << ok
        << "\n";

    return ok;
}





bool GameServer::ejectShipCockpitCapsule(EntityId id)
{
    const bool ok = m_simulation.ejectShipCockpitCapsule(id);

    std::cout
        << "[GameServer] ejectShipCockpitCapsule entityId="
        << id.value
        << " ok=" << ok
        << "\n";

    return ok;
}




bool GameServer::debugHangShipModule(EntityId id, const std::string& moduleId)
{
    return m_simulation.debugHangShipModule(id, moduleId);
}

bool GameServer::debugReevaluateShipStructure(EntityId id)
{
    return m_simulation.debugReevaluateShipStructure(id);
}



bool GameServer::debugRestoreShipModule(EntityId shipId, const std::string& moduleId)
{
    const bool ok = m_simulation.debugRestoreShipModule(shipId, moduleId);

    std::cout
        << "[GameServer] debugRestoreShipModule entityId="
        << shipId.value
        << " moduleId=" << moduleId
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}

bool GameServer::debugResetShipStructure(EntityId shipId)
{
    const bool ok = m_simulation.debugResetShipStructure(shipId);

    std::cout
        << "[GameServer] debugResetShipStructure entityId="
        << shipId.value
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}

void GameServer::debugResetAllShipStructures()
{
    m_simulation.debugResetAllShipStructures();

    std::cout << "[GameServer] debugResetAllShipStructures\n";
}









world::celestial::GalaxyMapSnapshot GameServer::buildGalaxyMapSnapshot() const
{
    world::celestial::GalaxyMapSnapshot out;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    out.universeDate =
        m_universeClock.dateTimeString();

    /*
        Stage 3C: Galaxy catalog geometry is deterministic client-owned data.
        The server sends only world-state overlays that are not part of the
        StarAtlas. Jurisdiction is sourced from authoritative initial/world
        state and keyed by physical system id; ClientMapService joins it with
        its local StarAtlas.
    */
    out.systems.reserve(m_systemJurisdictions.size());
    for (const auto& [systemId, jurisdiction] : m_systemJurisdictions)
    {
        world::celestial::GalaxyMapSystem overlay;
        overlay.id = systemId;
        overlay.jurisdiction = jurisdiction;
        out.systems.push_back(std::move(overlay));
    }

    std::sort(
        out.systems.begin(),
        out.systems.end(),
        [](const auto& a, const auto& b)
        {
            return a.id < b.id;
        }
    );

    return out;
}






namespace
{







}











void GameServer::setDiagnosticsSettings(
    const game::diagnostics::ServerDiagnosticsSettings& settings
)
{
    m_diagnostics.settings = settings;
    m_diagnostics.resetCaptureState();
}


const game::diagnostics::ServerDiagnosticsSettings&
GameServer::diagnosticsSettings() const
{
    return m_diagnostics.settings;
}


void GameServer::resetDiagnosticsCapture()
{
    m_diagnostics.resetCaptureState();
}


void GameServer::setDebugFastUniverseTime(bool enabled)
{
    setDebugUniverseTimeSimulation(
        enabled,
        m_debugFastUniverseTimeScale
    );
}

bool GameServer::debugFastUniverseTime() const
{
    return
        debugUniverseTimeSimulation() &&
        std::abs(
            debugUniverseTimeScale() -
            m_debugFastUniverseTimeScale
        ) < 0.0001;
}

void GameServer::setDebugUniverseTimeSimulation(
    bool enabled,
    double timeScale
)
{
    const bool wasEnabled =
        m_universeClock.simulationMode();
    const double previousEffectiveScale =
        m_universeClock.timeScale();

    if (enabled && !wasEnabled)
    {
        m_pendingUniverseTrajectoryDiagnosticEntry = true;
        m_pendingUniverseTrajectoryDiagnosticEpochSeconds =
            m_lastUniverseTimeSeconds;
    }
    else if (!enabled)
    {
        m_pendingUniverseTrajectoryDiagnosticEntry = false;
        m_pendingUniverseTrajectoryDiagnosticEpochSeconds = 0.0;
    }

    m_universeClock.setTimeScale(timeScale);
    m_universeClock.setSimulationMode(enabled);

    const bool timelineChanged =
        wasEnabled != m_universeClock.simulationMode() ||
        std::abs(
            previousEffectiveScale - m_universeClock.timeScale()
        ) > 1.0e-12;

    if (timelineChanged)
        ++m_universeTimelineRevision;

    m_debugFastUniverseTime = debugFastUniverseTime();

    // Publish the changed time contract on the next authoritative tick so
    // every client observes the new mode and scale without waiting for the
    // normal snapshot cadence.
    m_forceSnapshotPublication = true;
}

bool GameServer::debugUniverseTimeSimulation() const
{
    return m_universeClock.simulationMode();
}

double GameServer::debugUniverseTimeScale() const
{
    return m_universeClock.timeScale();
}

double GameServer::debugUniverseTimeConfiguredScale() const
{
    return m_universeClock.configuredTimeScale();
}


bool GameServer::terrainHeight(const std::string& bodyId, const glm::dvec3& worldDirection, GameSimulation::TerrainSample& ground)
{
    const auto* snapshot = celestialSnapshotForSystem(m_appliedSimulationContextSystemId);
    if (!snapshot)
        return false;
    game::navigation::globalflight::BodyTerrainSample g;
    if (!game::navigation::globalflight::sampleBodyTerrain(*snapshot, bodyId, worldDirection, g))
        return false;
    ground.heightM = g.heightM;
    ground.smoothHeightM = g.smoothHeightM;
    ground.material = g.material == game::navigation::globalflight::TerrainWaterMaterial ? GameSimulation::TerrainWater : g.material;
    ground.slopeDeg = g.slopeDeg;
    ground.feature = g.feature;
    return true;
}

// ---- navigation data (JUMP_MECHANICS 10k/10l): the colony bank, the onboard computer's memory, the data carriers.
// Data is not free: the price is logged, nothing is charged until the market exists (author 2026-11-13).
void GameServer::handleNavDataCommand(EntityId id, int op, const std::string& recordId, int carrier)
{
    namespace jj = game::navigation::jump;
    Ship* ship = m_simulation.getShip(id);
    if (!ship)
        return;
    auto& motion = ship->core().transform().motion;
    m_simulation.refreshNavComputerState(id);
    const bool computer = GameSimulation::navComputerWorks(*ship);
    auto records = jj::parseLaneRecords(motion.laneRecords);
    auto carriers = jj::parseDataCarriers(motion.dataCarriers);
    if (op == 2)
    {
        if (!computer || carrier < 1 || carrier > static_cast<int>(carriers.size()))
        {
            std::cerr << "[NavData] ship " << id.value << ": carrier " << carrier << " NOT read ("
                      << (computer ? "no such carrier" : "the onboard computer is down") << ")" << std::endl;
            return;
        }
        int stored = 0;
        for (const auto& r : carriers[static_cast<std::size_t>(carrier - 1)].records)
            stored += jj::storeLaneRecord(records, r) ? 1 : 0;
        motion.laneRecords = jj::formatLaneRecords(records);
        std::cerr << "[NavData] ship " << id.value << ": carrier '" << carriers[static_cast<std::size_t>(carrier - 1)].label
                  << "' read into the computer: " << stored << " new / fresher record(s), " << records.size() << " in memory" << std::endl;
        return;
    }
    const jj::NavDataBank* bank = nullptr;
    const jj::BankRecord* rec = jj::findBankRecord(recordId, &bank);
    const bool atHub = !motion.hubId.empty() && !motion.freeFlight;
    const char* why = nullptr;
    if (!rec || !bank)
        why = "no such record";
    else if (!atHub || jj::navDataBankInSystem(motion.systemId) != bank)
        why = "not docked at a hub of this bank";
    else if (rec->tier != jj::DataTier::Public)
        why = jj::dataTierText(rec->tier);
    else if (op == 0 && !computer)
        why = "the onboard computer is down";
    if (why)
    {
        std::cerr << "[NavData] ship " << id.value << ": record " << recordId << " REFUSED (" << why << ")" << std::endl;
        return;
    }
    const jj::LaneRecord lane = jj::laneRecordOf(m_starAtlas, *bank, *rec);
    std::string corridorName = std::to_string(rec->corridor);
    if (const auto* shell = jj::approachShellOf(m_starAtlas, rec->shellSystemId);
        shell && rec->corridor >= 1 && rec->corridor <= static_cast<int>(shell->corridors.size()))
        corridorName = shell->corridors[static_cast<std::size_t>(rec->corridor - 1)].name;
    if (op == 0)
    {
        const bool stored = jj::storeLaneRecord(records, lane);
        motion.laneRecords = jj::formatLaneRecords(records);
        std::cerr << "[NavData] ship " << id.value << ": " << bank->name << " record " << recordId << " (" << corridorName
                  << ", survey " << rec->surveyYear << ") " << (stored ? "into the computer" : "not newer than the one in memory")
                  << "; price " << rec->priceCr << " Cr (not charged: no market yet)" << std::endl;
    }
    else
    {
        jj::DataCarrier c;
        c.label = bank->name + ": " + corridorName;
        c.records = {lane};
        carriers.push_back(c);
        motion.dataCarriers = jj::formatDataCarriers(carriers);
        std::cerr << "[NavData] ship " << id.value << ": " << bank->name << " record " << recordId << " (" << corridorName
                  << ") onto carrier " << carriers.size() << "; price " << rec->priceCr << " Cr (not charged: no market yet)" << std::endl;
    }
}
