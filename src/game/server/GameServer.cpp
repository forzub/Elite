#include <atomic>
#include "src/model_asset/HubExpansion.h"
#include <cstdio>
#include <chrono>
#include <thread>

#include "src/game/navigation/rules/JumpExit.h"
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
#include "src/game/navigation/rules/JDrive.h"
#include <random>
#include "src/game/scene/GameSceneSetup.h"
#include "src/world/time/ConstructionClock.h"
#include "src/game/server/ServerTickProfile.h"
#include "src/game/ship/refit/ShipMarket.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"
#include "src/game/economy/EconomyModel.h"
#include "src/game/economy/EconomyService.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include <map>
#include <set>
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
        materializeWorldSiteHubs(initialSystemId, initialWorldState.orbitalHubs);

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

        // The economy (docs/ECONOMY_BANKING.md): national banks, one ledger per player (home bank = the polity
        // of the start system), the market state; installed for the server systems that charge (refuel service).
        if (m_economy.load())
        {
            m_economy.setUniverseTime(universeTime);   // a restored port call's berth time runs on this clock
            m_economy.start(game::economy::EconomyModel::dayOf(universeTime));
            for (const auto& [playerValue, player] : m_players.all())
                m_economy.ensurePlayer(playerValue, m_initialSimulationSystemId);
            game::economy::service::Bindings bindings;
            bindings.economy = &m_economy;
            bindings.playerOfShip = [this](std::uint32_t shipEntityId) -> std::uint64_t {
                const Ship* ship = m_simulation.getShip(EntityId{shipEntityId});
                return ship ? m_players.playerForShip(ship->core().registry().instanceId).value : 0;
            };
            bindings.systemOfHub = [this](const std::string& hubId) -> int {
                for (const auto& hub : m_lastSnapshot.hubs)
                    if (hub.id == hubId)
                        return hub.systemId;
                return -1;
            };
            bindings.locationOfShip = [this](std::uint32_t shipEntityId) -> std::pair<std::string, int> {
                const Ship* ship = m_simulation.getShip(EntityId{shipEntityId});
                if (!ship)
                    return {std::string(), -1};
                const auto& motion = ship->core().transform().motion;
                return {motion.hubId, motion.systemId};
            };
            // the ship reached its berth (GameSimulation::updatePortCall, the ship's system pass): hand in the contracts due
            // here whose cargo is aboard (TRADE_DESIGN §3.4) - the mass follows at once
            bindings.shipDocked = [this](std::uint32_t shipEntityId, const std::string& hubId) {
                const EntityId id{shipEntityId};
                const Ship* ship = m_simulation.getShip(id);
                if (!ship)
                    return;
                const std::uint64_t shipKey = ship->core().registry().instanceId;
                const std::uint64_t player = m_players.playerForShip(shipKey).value;
                if (player == 0)
                    return;
                if (m_economy.deliverContracts(player, shipKey, hubId, ship->core().transform().motion.systemId) > 0)
                {
                    (void)m_economy.takeCargoChange(shipKey);
                    applyCargoLoad(id);
                }
                // ship arrest (ECONOMY_BANKING 2026-11-20): a debt overdue at this polity's bank -> an exchange seizure
                const int systemId = ship->core().transform().motion.systemId;
                if (const auto claim = m_economy.arrestClaim(player, hubId, systemId); claim.due)
                    arrestShip(id, player, hubId, claim);
            };
            bindings.shipKeyOf = [this](std::uint32_t shipEntityId) -> std::uint64_t {
                const Ship* ship = m_simulation.getShip(EntityId{shipEntityId});
                return ship ? ship->core().registry().instanceId : 0;
            };
            game::economy::service::install(std::move(bindings));
            // a restored manifest (economy_state/2): the player ships carry its mass from the first tick
            for (const auto& [shipId, shipPtr] : m_simulation.ships())
                if (shipPtr && m_players.playerForShip(shipPtr->core().registry().instanceId))
                    applyCargoLoad(shipId);
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
GameServer::navigationStateForEntity(EntityId entityId, const ShipTransform* publishedTransform) const
{
    world::celestial::PlayerNavigationState navigation;

    const Ship* ship = m_simulation.getShip(entityId);
    if (!ship)
        return navigation;

    const ShipTransform tr = publishedTransform
        ? *publishedTransform
        : m_simulation.presentationShipTransform(entityId);

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

void GameServer::materializeWorldSiteHubs(int startSystemId,
                                          const std::vector<game::world_state::InitialWorldStateOrbitalHub>& authored)
{
    const auto& sites = world::celestial::WorldSitesCatalog::shared();   // loaded once per process
    if (!sites.loaded())
        return;
    int made = 0;
    int systems = 0;
    int authoredMade = 0;
    for (const auto& summary : m_starAtlas.systems())
    {
        const int systemId = summary.id;
        if (systemId == startSystemId)
            continue;
        const auto* system = m_starAtlas.findSystem(systemId);
        if (!system)
            continue;
        // the authored hubs of this system first (a colony hub with its city); their bodies get no generated station
        std::set<std::string> authoredBodies;
        bool switched = false;
        for (const auto& hub : authored)
        {
            if (hub.systemId != systemId)
                continue;
            if (!switched)
            {
                switchSimulationContext(systemId);
                switched = true;
            }
            if (game::scene::spawnOrbitalHub(m_simulation, hub))
            {
                ++authoredMade;
                authoredBodies.insert(hub.parentBodyId);
            }
            else
                std::cerr << "[WorldSiteHubs] authored hub " << hub.id << " in system " << systemId << ": not created" << std::endl;
        }
        // the gravity parameters of the system (radius, Kepler period) reach only the hubs that exist when they are
        // applied - switchSimulationContext applied them before these hubs: their period stayed 0 and a route to them
        // saw the hub at 29 000 km/s (2026-11-18 tour)
        if (switched)
            applyCelestialOrbitParentParameters(systemId);
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
            if (authoredBodies.count(s.parentBodyId))
                continue;   // the body's authored hub stands for it
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
        applyCelestialOrbitParentParameters(systemId);   // the generated hubs too (see above)
    }
    switchSimulationContext(startSystemId);
    std::cerr << "[WorldSiteHubs] " << made << " hubs in " << systems << " inhabited systems (one per body with world sites) + "
              << authoredMade << " authored hubs (colony cities)" << std::endl;
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
    // it runs before the command drain, in the last pass's system context: the placement reads the gravity bodies
    // (the star, "up") - the player's own system first (re-audit 2026-11-19: false [SystemContext] violations)
    switchSimulationContext(m_simulation.getShip(ship)->core().transform().motion.systemId);
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

// DEV: ELITE_DEV_JUMP_ENTRY_TIMING=1 - where a server tick's wall time goes around a system entry (a jump
// exit): every tick >= 25 ms, and every tick for 2 s after the set of simulated systems changed, as
// "[JumpEntryTiming] tick N total X ms: <phase>=<ms> ..." (phases >= 0.3 ms only).
struct JumpEntryTiming
{
    using Clock = std::chrono::steady_clock;
    const bool on = [] {
        const char* v = std::getenv("ELITE_DEV_JUMP_ENTRY_TIMING");
        return v && *v == '1';
    }();
    Clock::time_point start;
    Clock::time_point last;
    std::string line;
    int watchTicks = 0;
    std::vector<int> lastSystems;

    void begin()
    {
        if (!on)
            return;
        start = last = Clock::now();
        line.clear();
    }
    void mark(const std::string& phase)
    {
        if (!on)
            return;
        const auto now = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        if (ms >= 0.3)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "=%.1f", ms);
            line += " " + phase + buf;
        }
    }
    void systems(const std::vector<int>& passSystems)
    {
        if (!on)
            return;
        std::vector<int> sorted = passSystems;
        std::sort(sorted.begin(), sorted.end());
        if (sorted != lastSystems)
        {
            std::string list;
            for (const int s : sorted)
                list += (list.empty() ? "" : ",") + std::to_string(s);
            std::cerr << "[JumpEntryTiming] simulated systems now {" << list << "}" << std::endl;
            lastSystems = std::move(sorted);
            watchTicks = 100;
        }
    }
    void end(unsigned long long tick)
    {
        if (!on)
            return;
        const double total = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (total >= 25.0 || watchTicks > 0)
            std::cerr << "[JumpEntryTiming] tick " << tick << " total " << total << " ms:" << line << std::endl;
        if (watchTicks > 0)
            --watchTicks;
    }
};

JumpEntryTiming& jumpEntryTiming()
{
    static JumpEntryTiming timing;
    return timing;
}

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
    auto& entryTiming = jumpEntryTiming();
    entryTiming.begin();
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
    // DEV: ELITE_DEV_DESTROY_PLAYER_AT=<server s> destroys the player's ship once (live checks of the loss path)
    if (static const double destroyAt = [] {
            const char* v = std::getenv("ELITE_DEV_DESTROY_PLAYER_AT");
            return v && *v ? std::atof(v) : -1.0;
        }();
        destroyAt >= 0.0)
        if (static bool destroyed = false; !destroyed && m_serverTick * std::max(dt, 1.0e-6) >= destroyAt)
        {
            destroyed = true;
            std::cerr << "[Dev] destroying the player ship: " << m_simulation.devDestroyShip(m_simulation.playerId()) << std::endl;
        }
    // the hubs' construction stages follow the server's universe time (docs/HUB_CONSTRUCTION.md)
    world::time::setConstructionUniverseTime(m_universeClock.timeSeconds());
    game::scene::refreshHubCityConstructionCollision(m_simulation);
    entryTiming.mark("pre");

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
        // a contract failure (the economy's daily step) took cargo off this ship: the mass follows here, in its system pass
        if (m_economy.loaded() && m_economy.takeCargoChange(ship.core().registry().instanceId) && !time.universeTimeSimulation)
        {
            const int cargoSystemId = ship.core().transform().motion.systemId;
            if (cargoSystemId >= 0 && cargoSystemId != m_appliedSimulationContextSystemId)
                switchSimulationContext(cargoSystemId);
            applyCargoLoad(id);
        }
        // Trade requests (TRADE_DESIGN §7.4) run like the ship commands: in the context of THEIR ship's system, never
        // in accelerated diagnostics or for a destroyed ship
        if (auto ecoIt = m_pendingEconomyCommands.find(id.value); ecoIt != m_pendingEconomyCommands.end() && !ecoIt->second.empty())
        {
            if (!time.universeTimeSimulation && ship.core().transform().motion.destroyedCause == 0)
            {
                const int commandSystemId = ship.core().transform().motion.systemId;
                if (commandSystemId >= 0 && commandSystemId != m_appliedSimulationContextSystemId)
                    switchSimulationContext(commandSystemId);
                for (const auto& economyCommand : ecoIt->second)
                    handleEconomyCommand(id, economyCommand);
            }
            ecoIt->second.clear();
        }

        auto cmdIt = m_pendingClientShipCommands.find(id.value);
        if (cmdIt == m_pendingClientShipCommands.end())
            continue;

        auto& cmdQueue = cmdIt->second;

        if (time.universeTimeSimulation || ship.core().transform().motion.destroyedCause != 0)
        {
            cmdQueue.clear();   // accelerated diagnostics / a destroyed ship: no commands
            continue;
        }

        // A command runs in the context of ITS ship's system (audit 2026-11-19): the queue is drained before the
        // system passes, when the simulation still holds the last pass's system (gravity bodies, hubs, corridors).
        // toggleStabilization / toggleFreeFlight / jEngineBlocked / the jump procedure read that context - a ship
        // outside the last pass's system got a foreign primary body (live: a jump at HD 20794 zoned on the Sun).
        // DEV A/B: ELITE_DEV_NO_COMMAND_CONTEXT=1 restores the old behaviour (the violation counter must then fire)
        static const bool noCommandContext = [] {
            const char* v = std::getenv("ELITE_DEV_NO_COMMAND_CONTEXT");
            return v && *v == '1';
        }();
        if (!cmdQueue.empty() && !noCommandContext)
        {
            const int commandSystemId = ship.core().transform().motion.systemId;
            if (commandSystemId >= 0 && commandSystemId != m_appliedSimulationContextSystemId)
                switchSimulationContext(commandSystemId);
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
                    m_simulation.requestDockArrival(id, shipCmd.index != 0, shipCmd.target);
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
                            std::vector<game::navigation::rules::JumpExitCandidate> candidates;
                            for (const auto& b : system->bodies)
                                candidates.push_back({b.id, b.type == world::celestial::BodyType::Planet, b.radiusKm * 1000.0,
                                                      b.massKg, b.observed});
                            exitBody = game::navigation::rules::pickJumpExitBody(to->id, candidates);
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
                        // the approach shells' own currents on the way (Fortuna, docs/FORTUNA_LEXIE.md)
                        deepCatch.shellDriftM = jj::interstellarShellDriftM(m_starAtlas, route, universeS);
                        if (glm::length(deepCatch.shellDriftM) > 0.0)
                            std::cerr << "[Jump] the way to " << to->name << ": approach-shell currents "
                                      << glm::length(deepCatch.shellDriftM) / 1000.0 << " km (before the lock " << lock << ")" << std::endl;
                    }
                    m_simulation.requestJump(id, to ? to->id : -1, targetName, pathLy, exitBody, dir,
                                             deepCatch.placeId >= 0 || glm::length(deepCatch.shellDriftM) > 0.0 ? &deepCatch : nullptr);
                    break;
                }

                case ClientShipCommand::NavData:
                    handleNavDataCommand(id, shipCmd.index, shipCmd.detail, static_cast<int>(shipCmd.amount));
                    break;

                case ClientShipCommand::RequestRefuel:
                    // docked at a station (or held in a hub's frame): fill the jump charge / stop (docs/FUEL_STATIONS.md)
                    if (shipCmd.index == 0)
                        m_simulation.cancelRefuel(id, "the pilot stopped it");
                    else
                        (void)m_simulation.requestRefuel(id, shipCmd.target);
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

                case ClientShipCommand::HireJob:     // F5 employment service: spawns + boards - after the drain
                    m_pendingHires.push_back({id, shipCmd.detail.empty() ? std::string("?") : shipCmd.detail});
                    break;

                case ClientShipCommand::QuitJob:
                    m_pendingHires.push_back({id, std::string()});
                    break;

                case ClientShipCommand::DevFullRestore:   // TEMPORARY (author 2026-11-21): Ctrl+Alt+R
                    m_simulation.debugFullRestoreShip(id);
                    break;

                case ClientShipCommand::EnterHubFrame:   // a docking request near a hub in normal flight
                    (void)m_simulation.enterHubFrameForModule(id, shipCmd.target);
                    break;

                case ClientShipCommand::ToggleLandingGear:   // G: the landing legs
                    m_simulation.toggleLandingGear(id);
                    break;

                case ClientShipCommand::LandOnPad:   // a hub city pad: land (index 1) / the pilot takes over (0)
                    (void)m_simulation.requestPadLanding(id, shipCmd.target, shipCmd.detail, shipCmd.index != 0);
                    break;

                case ClientShipCommand::BuyShip:     // F6: spawns a ship - after the drain too
                    m_pendingPurchases.push_back({id, shipCmd.detail, shipCmd.target});
                    break;

                case ClientShipCommand::BoardShip:   // base 2: executed after the drain (the queues are reset)
                    m_pendingBoardings.push_back({id, EntityId{static_cast<std::uint32_t>(std::max(0.0, shipCmd.amount))}});
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
                    else if (const std::string why = chargeRefit(id, subject, op); !why.empty())
                        m_simulation.refuseRefit(id, subject, why);
                    else
                    {
                        m_simulation.applyRefit(id, subject, op);
                        m_shipsDirty = true;
                    }
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



    for (const auto& [from, vacancy] : m_pendingHires)
    {
        const std::string why = vacancy.empty() ? quitJob(from) : hireJob(from, vacancy);
        std::cerr << "[Employment] ship " << from.value << (vacancy.empty() ? " quits" : " hires " + vacancy) << ": "
                  << (why.empty() ? std::string("DONE") : "REFUSED: " + why) << std::endl;
    }
    m_pendingHires.clear();

    for (const auto& p : m_pendingPurchases)
    {
        const std::string why = buyShip(p.from, p.passportId, p.offerId);
        std::cerr << "[Shipyard] ship " << p.from.value << " buys " << p.passportId << " (" << p.offerId << "): "
                  << (why.empty() ? std::string("DONE") : "REFUSED: " + why) << std::endl;
    }
    m_pendingPurchases.clear();

    // rescue (step 12): a pilot whose ship reached the hub of the accepted SOS call completes it
    // DEV: ELITE_DEV_LANE_CHECK=<hub id> - the lane tubes vs the hub's collision, once the city volumes exist
    if (m_serverTick == 1800)
        if (const char* hub = std::getenv("ELITE_DEV_LANE_CHECK"); hub && *hub)
            m_simulation.devLaneClearanceReport(hub);
    // ship persistence: restore once the hubs exist; save on a change and every ~minute
    restoreRuntimeShipsIfDue();
    if (!m_runtimeShips.empty() && (m_shipsDirty || m_serverTick - m_shipsSavedTick >= 3600))
        saveRuntimeShips();
    // the world's hubs (a route can reach them) anchor the SOS calls; refreshed when their count changes
    if (m_economy.loaded() && m_serverTick % 30 == 0 && m_lastSnapshot.hubs.size() != m_economy.routableHubCount())
    {
        std::set<std::string> hubs;
        for (const auto& hub : m_lastSnapshot.hubs)
            hubs.insert(hub.id);
        m_economy.setRoutableHubs(std::move(hubs));
        // step 16: per world hub with an expansion plan, its first PLANNED project with a 4-stage kit (the contract target)
        std::map<std::string, game::economy::EconomyServer::PlannedProject> buildable;
        for (const auto& hub : m_lastSnapshot.hubs)
            if (const auto* plan = elite::model_asset::hubExpansionPlan(hub.id))
            {
                const int today = elite::model_asset::hubExpansionDay(*plan, m_universeClock.timeSeconds());
                std::string id, kind;
                int start = 1 << 30;
                for (const auto& p : plan->projects)
                    if (p.startDay > today && p.startDay < start &&
                        (p.kind == "hall" || p.kind == "yard" || p.kind == "slab" || p.kind == "hab" || p.kind == "tower" || p.kind == "dock"))
                    {
                        start = p.startDay;
                        id = p.id;
                        kind = p.kind;
                    }
                if (!id.empty())
                    buildable[hub.id] = {id, kind};
            }
        m_economy.setBuildableProjects(std::move(buildable));
    }
    // step 16: the player's contracts drawn as the hubs' own projects (start / work days / halts in the plan's days)
    if (m_economy.loaded() && m_economy.buildRevision() != m_appliedBuildRevision)
    {
        m_appliedBuildRevision = m_economy.buildRevision();
        for (const auto& p : m_economy.buildProjects())
            if (p.offer.projectId.rfind("site:", 0) == 0)
                continue;   // a mine rig: not drawn (the mine hub meshes are not touched)
            else if (const auto* plan = elite::model_asset::hubExpansionPlan(p.offer.hubId))
            {
                const int planToday = elite::model_asset::hubExpansionDay(*plan, m_universeClock.timeSeconds());
                const int offset = planToday - m_economy.day();   // economy day -> the plan's day
                std::vector<elite::model_asset::ExpansionProject::Halt> halts;
                for (const auto& [from, to] : p.halts)
                    halts.push_back({from + offset, to < 0 ? -1 : to + offset});
                if (p.status == "abandoned" && (halts.empty() || halts.back().to >= 0))
                    halts.push_back({planToday, -1});
                const bool ok = elite::model_asset::setHubExpansionProjectOverride(p.offer.hubId, p.offer.projectId, p.startDay + offset,
                                                                             p.offer.workDays, halts);
                std::cerr << "[Build] drawing " << p.offer.projectId << " at " << p.offer.hubId << ": start " << (p.startDay + offset) << ", "
                          << p.offer.workDays << " days, " << halts.size() << " halt(s) -> " << (ok ? "ok" : "NO PLAN") << std::endl;
            }
    }
    if (m_economy.loaded() && m_serverTick % 30 == 0)
        for (const EntityId pid : m_simulation.playerControlledShipIds())
            if (const Ship* ps = m_simulation.getShip(pid))
            {
                const std::uint64_t player = m_players.playerForShip(ps->core().registry().instanceId).value;
                const auto& motion = ps->core().transform().motion;
                if (player != 0 && !motion.hubId.empty())
                    (void)m_economy.completeSosAt(player, motion.hubId, motion.systemId);
            }

    // ship changes asked this tick (base 2): after the drain - they reset the per-entity queues
    for (const auto& [from, to] : m_pendingBoardings)
        if (const Ship* fromShip = m_simulation.getShip(from))
        {
            const PlayerId player = m_players.playerForShip(fromShip->core().registry().instanceId);
            const std::string why = player ? transferPlayerControl(player, to) : std::string("нет пилота");
            std::cerr << "[Board] ship " << from.value << " -> " << to.value << ": " << (why.empty() ? std::string("DONE") : "REFUSED: " + why)
                      << std::endl;
        }
    m_pendingBoardings.clear();

    // Player movement is fully driven by the ordinary numbered
    // ShipControlState stream. Navigation/autopilot lives on the client and
    // is indistinguishable here from human control input.

entryTiming.mark("commands");
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
    entryTiming.systems(passSystems);
    for (std::size_t pass = 0; pass < passSystems.size(); ++pass)
    {
        const std::string sys = std::to_string(passSystems[pass]);
        switchSimulationContext(passSystems[pass]);
        entryTiming.mark("ctx" + sys);
        double stageBefore[7] = {};
        if (entryTiming.on)
            std::copy(std::begin(game::server::serverTickProfile().simStage), std::end(game::server::serverTickProfile().simStage), stageBefore);
        m_simulation.setSystemPass(static_cast<int>(pass));
        m_simulation.update(time);
        if (entryTiming.on)
        {
            // the pass's GameSimulation::update stages (ServerTickProfile::simStage)
            std::string stages;
            for (int k = 0; k < 7; ++k)
            {
                const double ms = (game::server::serverTickProfile().simStage[k] - stageBefore[k]) * 1000.0;
                if (ms >= 0.3)
                {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%sst%d:%.1f", stages.empty() ? "" : ",", k, ms);
                    stages += buf;
                }
            }
            entryTiming.mark("sim" + sys + (stages.empty() ? "" : "[" + stages + "]"));
        }
    }
    m_simulation.setSystemPass(0);
    m_lastTickSystemPasses = passSystems.size();

    // A player ship lost (destroyed / disabled in combat / crushed): its cargo leaves the manifest ONCE, into lost-cargo
    // records (EconomyServer::recordShipLoss, idempotent by event id) - it must not come back with the ship after a
    // restart, nor stay both aboard and lost (re-audit 2026-11-19; docs/CARGO_SALVAGE_DESIGN.md minimal part)
    if (m_economy.loaded())
        for (const EntityId id : m_simulation.playerControlledShipIds())
        {
            const Ship* lost = m_simulation.getShip(id);
            if (!lost || lost->core().transform().motion.destroyedCause == 0 || !m_reportedShipLosses.insert(id.value).second)
                continue;
            const auto& tr = lost->core().transform();
            game::economy::EconomyServer::ShipLoss loss;
            loss.shipKey = lost->core().registry().instanceId;
            loss.eventId = "loss:" + std::to_string(loss.shipKey) + ":" + std::to_string(m_serverTick);
            loss.systemId = tr.motion.systemId;
            const glm::dvec3 at = tr.fullWorldMeters();
            loss.worldM[0] = at.x;
            loss.worldM[1] = at.y;
            loss.worldM[2] = at.z;
            loss.universeS = m_universeClock.timeSeconds();
            static const char* const causes[] = {"-", "surface impact", "atmosphere", "disabled in combat", "crushed"};
            const int c = tr.motion.destroyedCause;
            loss.cause = c >= 0 && c <= 4 ? causes[c] : "destroyed";
            m_economy.recordShipLoss(loss);
            applyCargoLoad(id);
        }
    game::server::serverTickProfile().systemPasses += static_cast<long long>(passSystems.size());
    game::server::serverTickProfile().simulatedShips += static_cast<long long>(m_simulation.ships().size());
    m_lastTickSimulationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - passStart).count();
    game::server::serverTickProfile().simulation += m_lastTickSimulationSeconds;
    entryTiming.mark("losses");
}

// Navigation sensors observe the completed authoritative epoch. Their cadence
// is device-owned and uses universe time; render/snapshot FPS cannot create a
// measurement. The synthetic radar asks for truth rows only when a scan is due.
{
    game::server::ScopedTickTimer timer(game::server::serverTickProfile().sensors);
    updateNavigationSensorDevices(universeTime);
    entryTiming.mark("sensors");
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
            profile.economyQuotes += static_cast<long long>(quotes.size() + events.size());
        }
        if (economyNext >= hubs.size())
        {
            economyNext = 0;
            ++economyDayOffset;
            ++profile.economyTicks;
        }
    }
}
// The world changes once a day (author 2026-11-19): banks (FX, interest, due dates, standing) and every
// market's stock / price take one daily step per world day passed (<= 3 per tick when the clock races).
{
    game::server::ScopedTickTimer timer(game::server::serverTickProfile().economy);
    m_economy.advanceTo(game::economy::EconomyModel::dayOf(universeTime) + m_devEconomyDayOffset);
    m_economy.setClockDays(universeTime / 86400.0 + m_devEconomyDayOffset);
    m_economy.setUniverseTime(universeTime);
    entryTiming.mark("economy");
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
    entryTiming.mark("snapshot+maps");
    entryTiming.end(m_serverTick);
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
    snapshot.session.wallet = {};
    snapshot.session.trade = {};
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
    m_pendingEconomyCommands.erase(controlledEntityId.value);

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

void GameServer::configurePlayerRadar(EntityId entity, ShipInstanceId instance)
{
    Ship* ship = m_simulation.getShip(entity);
    if (!ship)
        return;
    const auto& radar = ship->core().radar();
    const auto& desc = radar.getDesc();
    if (desc.backendKind != game::RadarBackendKind::TestIdeal)
        return;
    auto& runtime = m_playerRadarRuntimes[instance];
    if (!runtime.configured)
    {
        runtime.unit.configure(desc, instance);
        runtime.configured = true;
    }
    runtime.radarOperational = radar.isOperational() && radar.getAvailablePower() + 1.0e-9 >= desc.powerConsumption;
    const auto& transform = ship->core().transform();
    runtime.navigationSolution =
        runtime.unit.navigationSolution(m_universeClock.timeSeconds(), transform.worldPosition, transform.motion.worldVelocityMps);
    runtime.hasNavigationSolution = true;
}

std::string GameServer::hireJob(EntityId from, const std::string& vacancyId)
{
    Ship* ship = m_simulation.getShip(from);
    if (!ship || !m_economy.loaded())
        return "нет корабля или экономики";
    const PlayerId player = m_players.playerForShip(ship->core().registry().instanceId);
    if (!player)
        return "нет пилота";
    auto refuse = [&](const std::string& why) {
        m_economy.recordTradeRefusal(player.value, 0, "employment", why);
        return why;
    };
    bool tender = false;
    const std::string hub = m_simulation.serviceStationOf(from, &tender);
    if (hub.empty() || tender)
        return refuse("наём — у причала хаба (служба занятости)");
    const int system = ship->core().transform().motion.systemId;
    const auto list = m_economy.vacanciesAt(hub, system);
    const game::economy::EconomyServer::Vacancy* v = nullptr;
    for (const auto& x : list)
        if (x.id == vacancyId)
            v = &x;
    if (!v)
        return refuse("вакансия закрыта");
    if (const std::string why = m_economy.vacancyRefusal(player.value, *v, hub, system); !why.empty())
        return refuse(why);
    const ShipDescriptor* d = game::ship::descriptors::findCatalogShip(v->passportId);
    if (!d)
        return refuse("нет корабля работодателя в каталоге: " + v->passportId);
    ShipInitData init;
    init.visual.shipType = d->identity.shipType;
    init.visual.shipName = d->identity.shipName;
    init.registry.instanceId = ++m_nextBoughtShipInstance;
    init.registry.ownerName = v->employerRu;
    init.registry.ownerActor = 0x7E000000u + static_cast<ActorId>(std::hash<std::string>{}(v->employerRu) & 0xFFFFu);   // the employer
    init.registry.registrationId = d->catalog.passportId + "-" + std::to_string(init.registry.instanceId);
    init.registry.shipRole = v->kind == "military" ? ShipRoleType::Military : ShipRoleType::Civilian;
    const EntityId employer = m_simulation.spawnShipBeside(from, ShipRole::Player, *d, init, 5000.0);
    if (employer.value == 0)
        return refuse("работодатель не выдал корабль");
    m_hires[player.value] = {from, employer};
    m_runtimeShips.insert(init.registry.instanceId);
    m_shipsDirty = true;
    if (const std::string why = transferPlayerControl(player, employer); !why.empty())
    {
        m_hires.erase(player.value);
        return refuse("не удалось пересесть: " + why);
    }
    m_economy.startEmployment(player.value, *v);
    m_economy.recordOperation(player.value, "hired", "принят: " + v->roleRu + " — " + v->employerRu + ", корабль " + v->passportId, 0.0);
    return {};
}

std::string GameServer::quitJob(EntityId from)
{
    Ship* ship = m_simulation.getShip(from);
    const PlayerId player = ship ? m_players.playerForShip(ship->core().registry().instanceId) : PlayerId{};
    if (!player)
        return "нет пилота";
    const auto it = m_hires.find(player.value);
    if (it == m_hires.end())
    {
        m_economy.endEmployment(player.value, "без корабля работодателя");
        return {};
    }
    if (const std::string why = transferPlayerControl(player, it->second.ownShip); !why.empty())
    {
        m_economy.recordTradeRefusal(player.value, 0, "employment", "вернуться на свой корабль: " + why);
        return why;
    }
    m_hires.erase(it);
    m_economy.endEmployment(player.value, "по собственному желанию");
    m_economy.recordOperation(player.value, "quit", "уволился: снова на своём корабле", 0.0);
    return {};
}

std::string GameServer::buyShip(EntityId from, const std::string& passportId, const std::string& offerId)
{
    Ship* ship = m_simulation.getShip(from);
    if (!ship || !m_economy.loaded())
        return "нет корабля или экономики";
    const std::uint64_t player = m_players.playerForShip(ship->core().registry().instanceId).value;
    bool tender = false;
    const std::string hub = m_simulation.serviceStationOf(from, &tender);
    auto refuse = [&](const std::string& why) {
        if (player)
            m_economy.recordTradeRefusal(player, 0, "ship_purchase", why);
        return why;
    };
    if (player == 0)
        return "нет пилота";
    if (hub.empty() || tender)
        return refuse("корабль покупают у причала хаба");
    const auto* design = game::catalog::ships::ShipDesignCatalog::instance().findShip(passportId);
    const ShipDescriptor* d = game::ship::descriptors::findCatalogShip(passportId);
    if (!design || !d)
        return refuse("нет такой модели: " + passportId);
    const auto offers = game::ship::refit::devShipOffers(*design);
    const game::ship::refit::ShipOffer* offer = nullptr;
    for (const auto& o : offers)
        if (o.offerId == offerId)
            offer = &o;
    if (!offer)
        return refuse("нет такого предложения");
    if (offer->condition != "new")
        return refuse("подержанные корабли — позже: износ и повреждения ещё не переносятся на корабль");
    const int system = ship->core().transform().motion.systemId;
    const std::string name = design->name.empty() ? passportId : design->designation + " «" + design->name + "»";
    const auto paid = m_economy.chargeService(player, hub, system, game::economy::Purpose::Other, offer->priceCr, "покупка корабля " + name);
    if (!paid.ok)
        return refuse("не хватает денег: " + std::to_string(static_cast<long long>(offer->priceCr)) + " Cr (" + paid.reason + ")");
    ShipInitData init;
    init.visual.shipType = d->identity.shipType;
    init.visual.shipName = d->identity.shipName;
    init.registry.instanceId = ++m_nextBoughtShipInstance;
    init.registry.ownerName = "player";
    init.registry.ownerActor = ship->core().registry().ownerActor;
    init.registry.registrationId = d->catalog.passportId + "-" + std::to_string(init.registry.instanceId);
    init.registry.shipRole = ShipRoleType::Civilian;
    const EntityId id = m_simulation.spawnShipBeside(from, ShipRole::Player, *d, init, 5000.0);
    if (id.value == 0)
    {
        m_economy.credit(player, hub, system, offer->priceCr, "возврат: корабль не создан");
        return refuse("верфь не смогла выдать корабль (деньги возвращены)");
    }
    m_runtimeShips.insert(init.registry.instanceId);
    m_shipsDirty = true;
    m_economy.recordOperation(player, "ship_bought", "куплен корабль " + name + " за " + std::to_string(static_cast<long long>(offer->priceCr)) +
                                                         " Cr: ждёт в 5 км от хаба (F7 -> ПЕРЕСЕСТЬ)", -offer->priceCr);
    return {};
}

std::string GameServer::transferPlayerControl(PlayerId playerId, EntityId target, bool restoring)
{
    const PlayerState* player = m_players.find(playerId);
    const EntityId from = m_controls.controlledEntity(playerId);
    Ship* fromShip = m_simulation.getShip(from);
    Ship* toShip = m_simulation.getShip(target);
    if (!player || !fromShip || !toShip || from == target)
        return "нет такого корабля";
    auto& to = toShip->core();
    if (to.role() != ShipRole::Player)
        return "корабль не подготовлен для пилота (роль NPC)";
    const auto hireIt = m_hires.find(playerId.value);
    const bool employerShip = hireIt != m_hires.end() && hireIt->second.employerShip == target;
    if (!employerShip && (to.registry().ownerActor == 0 || to.registry().ownerActor != player->actor))
        return "чужой корабль";
    if (to.transform().motion.destroyedCause != 0)
        return "корабль разрушен";
    if (m_simulation.isPlayerControlled(target) || m_players.playerForShip(to.registry().instanceId))
        return "кораблём уже управляют";
    const auto& a = fromShip->core().transform();
    const auto& b = to.transform();
    // two ships at the same station are "here" by its frame (a demoted, uncontrolled ship keeps a stale world position while
    // the station orbits - found live: 533 km after 3 days); elsewhere the world distance
    const bool sameStation = !a.motion.hubId.empty() && a.motion.hubId == b.motion.hubId;
    if (!restoring && (a.motion.systemId != b.motion.systemId || (!sameStation && glm::length(a.fullWorldMeters() - b.fullWorldMeters()) > 20000.0)))
        return "корабль далеко (нужно <= 20 км в той же системе): " + std::to_string(static_cast<long long>(glm::length(a.fullWorldMeters() - b.fullWorldMeters()))) +
               " м, системы " + std::to_string(a.motion.systemId) + " / " + std::to_string(b.motion.systemId);
    // the persistent identity of a ship spawned after the bootstrap
    ShipInstanceId instance = m_shipInstances.instanceForEntity(target);
    if (instance == 0)
    {
        game::server::ShipInstanceRecord record;
        record.instanceId = to.registry().instanceId;
        record.materializedEntityId = target;
        record.typeId = to.desc().typeId;
        record.catalogShipId = to.desc().catalog.passportId;
        record.roleType = to.registry().shipRole;
        record.ownerActor = to.registry().ownerActor;
        record.name = to.visualIdentity().shipName;
        record.registrationId = to.registry().registrationId;
        if (!m_shipInstances.registerMaterialized(std::move(record)))
            return "реестр кораблей отказал";
        instance = m_shipInstances.instanceForEntity(target);
    }
    if (!employerShip)
        (void)m_shipOwnership.assign(instance, game::server::ShipOwnerRef::player(playerId));   // the employer keeps its ship
    resetSessionControlState(from, "ship-change-out");
    m_simulation.setPlayerControlled(from, false);
    if (!m_controls.rebindHuman(playerId, target))
    {
        m_simulation.setPlayerControlled(from, true);
        return "управление не передано";
    }
    if (!m_players.assignShip(playerId, instance))
    {
        (void)m_controls.rebindHuman(playerId, from);
        m_simulation.setPlayerControlled(from, true);
        return "реестр пилотов отказал";
    }
    resetSessionControlState(target, "ship-change-in");
    m_simulation.setPlayerControlled(target, true);
    configurePlayerRadar(target, instance);
    ++m_controlEpochs[playerId.value];
    m_shipsDirty = true;   // who flies what is saved with the ships
    return {};
}

void GameServer::saveRuntimeShips()
{
    m_shipsDirty = false;
    m_shipsSavedTick = m_serverTick;
    const std::string path = m_economy.stateFilePath();
    if (path.empty())
        return;
    nlohmann::json j;
    j["schema"] = "runtime_ships/1";
    j["next_instance"] = m_nextBoughtShipInstance;
    j["ships"] = nlohmann::json::array();
    for (const auto& [eid, sp] : m_simulation.ships())
    {
        if (!sp || !m_runtimeShips.count(sp->core().registry().instanceId) || sp->core().transform().motion.destroyedCause != 0)
            continue;
        const auto& core = sp->core();
        const auto& tr = core.transform();
        const PlayerId flier = m_players.playerForShip(core.registry().instanceId);
        std::string employerOf;
        for (const auto& [player, hire] : m_hires)
            if (hire.employerShip == eid)
                employerOf = std::to_string(player);
        j["ships"].push_back({{"instance", core.registry().instanceId}, {"passport", core.desc().catalog.passportId},
                              {"overrides", core.productOverrides()}, {"owner_actor", core.registry().ownerActor},
                              {"owner_name", core.registry().ownerName}, {"registration", core.registry().registrationId},
                              {"name", core.visualIdentity().shipName}, {"military", core.registry().shipRole == ShipRoleType::Military},
                              {"system", tr.motion.systemId}, {"hub", tr.motion.hubId},
                              {"local", {tr.motion.localPositionMeters.x, tr.motion.localPositionMeters.y, tr.motion.localPositionMeters.z}},
                              {"flown_by", flier ? flier.value : 0}, {"employer_of", employerOf}});
    }
    // the own ships of hired pilots (to go back to)
    j["hires"] = nlohmann::json::array();
    for (const auto& [player, hire] : m_hires)
    {
        const Ship* own = m_simulation.getShip(hire.ownShip);
        const Ship* emp = m_simulation.getShip(hire.employerShip);
        if (own && emp)
            j["hires"].push_back({player, own->core().registry().instanceId, emp->core().registry().instanceId});
    }
    if (!game::economy::writeFileAtomically(path + ".ships.json", j.dump(1)))
        std::cerr << "[ShipSave] NOT saved: " << path << ".ships.json" << std::endl;
}

void GameServer::restoreRuntimeShipsIfDue()
{
    if (m_shipsRestored || m_lastSnapshot.hubs.empty())
        return;
    m_shipsRestored = true;
    const std::string path = m_economy.stateFilePath();
    if (path.empty())
        return;
    std::ifstream f(path + ".ships.json", std::ios::binary);
    if (!f)
        return;
    nlohmann::json j;
    try
    {
        f >> j;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ShipSave] restore failed: " << e.what() << std::endl;
        return;
    }
    if (j.value("schema", std::string()) != "runtime_ships/1")
        return;
    m_nextBoughtShipInstance = std::max(m_nextBoughtShipInstance, j.value("next_instance", m_nextBoughtShipInstance));
    std::unordered_map<std::uint64_t, EntityId> byInstance;
    for (const auto& [eid, sp] : m_simulation.ships())
        if (sp)
            byInstance[sp->core().registry().instanceId] = eid;
    std::vector<std::pair<std::uint64_t, EntityId>> flown;
    for (const auto& s : j.value("ships", nlohmann::json::array()))
    {
        const std::string passport = s.value("passport", std::string());
        const ShipDescriptor* base = game::ship::descriptors::findCatalogShip(passport);
        if (!base)
            continue;
        const auto overrides = s.value("overrides", std::vector<std::string>());
        const ShipDescriptor* d = overrides.empty() ? base : game::ship::refit::configuredDescriptor(*base, overrides);
        if (!d)
            d = base;
        ShipInitData init;
        init.visual.shipType = d->identity.shipType;
        init.visual.shipName = s.value("name", d->identity.shipName);
        init.registry.instanceId = s.value("instance", std::uint64_t{0});
        init.registry.ownerName = s.value("owner_name", std::string());
        init.registry.ownerActor = s.value("owner_actor", ActorId{0});
        init.registry.registrationId = s.value("registration", std::string());
        init.registry.shipRole = s.value("military", false) ? ShipRoleType::Military : ShipRoleType::Civilian;
        const auto local = s.value("local", std::vector<double>{0.0, 0.0, 0.0});
        const EntityId id = m_simulation.spawnShipAtHub(ShipRole::Player, *d, init, s.value("system", -1), s.value("hub", std::string()),
                                                        local.size() == 3 ? glm::dvec3(local[0], local[1], local[2]) : glm::dvec3(0.0));
        if (id.value == 0)
        {
            std::cerr << "[ShipSave] restore: " << passport << " at '" << s.value("hub", std::string()) << "' skipped (no hub frame)" << std::endl;
            continue;
        }
        if (Ship* ship = m_simulation.getShip(id))
            for (const auto& o : overrides)
                if (const auto eq = o.find('='); eq != std::string::npos)
                    ship->core().recordProductOverride(o.substr(0, eq), o.substr(eq + 1));
        m_runtimeShips.insert(init.registry.instanceId);
        byInstance[init.registry.instanceId] = id;
        if (const std::uint64_t p = s.value("flown_by", std::uint64_t{0}); p != 0)
            flown.push_back({p, id});
        std::cerr << "[ShipSave] restored " << passport << " (" << init.registry.instanceId << ") at " << s.value("hub", std::string()) << " -> entity "
                  << id.value << std::endl;
    }
    for (const auto& h : j.value("hires", nlohmann::json::array()))
    {
        const std::uint64_t player = h.at(0).get<std::uint64_t>();
        const auto own = byInstance.find(h.at(1).get<std::uint64_t>());
        const auto emp = byInstance.find(h.at(2).get<std::uint64_t>());
        if (own != byInstance.end() && emp != byInstance.end())
            m_hires[player] = {own->second, emp->second};
    }
    for (const auto& [player, id] : flown)
    {
        const std::string why = transferPlayerControl(PlayerId{player}, id, true);
        std::cerr << "[ShipSave] the pilot back in ship " << id.value << ": " << (why.empty() ? std::string("DONE") : why) << std::endl;
    }
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
            else if constexpr (std::is_same_v<PayloadT, game::network::ClientEconomyCommand>)
            {
                // trade requests (TRADE_DESIGN §7.4): queued per controlled ship, run in its system context
                auto& queue = m_pendingEconomyCommands[controlledEntityId.value];
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

            // the hub cities' traffic not made real, the stray bodies, the debris field - the nearest 128 (author
            // 2026-11-22: «радар должен показывать всё, даже обломки»; «если есть ограничение по количеству - то ближайшие»)
            const auto hubContacts = m_simulation.hubRadarContacts(
                world::coordinates::fullMeters(observerTransform.worldPosition), observerSystemId, 128);
            {
                static double logAt = -1.0e9;
                if (universeTimeSeconds - logAt >= 15.0 && !hubContacts.empty())
                {
                    logAt = universeTimeSeconds;
                    int debris = 0, nearestDebris = -1;
                    double best = 1.0e30;
                    const glm::dvec3 me = world::coordinates::fullMeters(observerTransform.worldPosition);
                    for (const auto& k : hubContacts)
                        if (k.kind == game::radar::RadarTrackKindDebris)
                        {
                            ++debris;
                            best = std::min(best, glm::length(k.worldPosition - me));
                            nearestDebris = 1;
                        }
                    std::cerr << "[Radar] player radar: " << truth.size() << " ships / structures + " << hubContacts.size()
                              << " hub contacts (" << debris << " debris / stray, nearest "
                              << (nearestDebris > 0 ? static_cast<long long>(best) : -1) << " m)" << std::endl;
                }
            }
            for (const auto& k : hubContacts)
            {
                game::radar::TestIdealRadarTruthContact row;
                row.sourceKey = 0x3000000000000000ULL | static_cast<std::uint64_t>(k.key);
                row.worldPosition = world::coordinates::makeWorldPositionFromMeters(k.worldPosition);
                row.worldVelocityMps = k.worldVelocity;
                row.radarCrossSection = k.radarCrossSection;
                row.kind = k.kind;
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


game::simulation::ClientWalletSnapshot
GameServer::walletForSession(
    game::network::ServerSessionId sessionId
) const
{
    const PlayerId player = m_sessions.player(sessionId);
    if (!player || !m_economy.loaded())
        return {};
    std::string hubId;
    int systemId = -1;
    if (const Ship* ship = m_simulation.getShip(controlledEntityForSession(sessionId)))
    {
        hubId = ship->core().transform().motion.hubId;
        systemId = ship->core().transform().motion.systemId;
    }
    return m_economy.walletSnapshot(player.value, hubId, systemId);
}

namespace
{
game::economy::CargoCapacity cargoCapacityOf(const Ship& ship)
{
    const auto& storage = ship.core().descriptor().storage;
    return {static_cast<double>(std::max(0, storage.cargoMass)), std::max(0, storage.containerSlots)};
}

// one full J charge at `massT` (JDrive: charge = q x d x (alpha + M))
double jumpRangeLy(double chargeT, double massT)
{
    namespace jd = game::navigation::jdrive;
    return chargeT > 0.0 ? chargeT / (jd::JumpChargePerLyT * (jd::JumpEnvelopeT + std::max(0.0, massT))) : 0.0;
}
} // namespace

void GameServer::applyCargoLoad(EntityId id)
{
    Ship* ship = m_simulation.getShip(id);
    if (!ship)
        return;
    const auto* manifest = m_economy.cargo().find(ship->core().registry().instanceId);
    auto load = ship->core().load();
    const double net = manifest ? manifest->netT() : 0.0;
    const int boxes = manifest ? manifest->c20() : 0;
    if (std::fabs(load.cargoTonnes - net) < 1.0e-9 && load.containers == boxes)
        return;
    const double before = ship->core().currentMassKg() / 1000.0;
    load.cargoTonnes = net;
    load.containers = boxes;
    ship->core().setLoad(load);
    const double after = ship->core().currentMassKg() / 1000.0;
    const double chargeT = ship->core().effectivePhysics().jumpChargeCapacityT;
    std::cerr << "[Cargo] ship " << id.value << ": manifest " << net << " t net, " << boxes << " C20 (gross "
              << (manifest ? manifest->grossT() : 0.0) << " t) -> mass " << before << " -> " << after << " t; full-charge jump range "
              << jumpRangeLy(chargeT, before) << " -> " << jumpRangeLy(chargeT, after) << " ly" << std::endl;
}

// Exchange seizure (ZPO §811 Austauschpfändung, ECONOMY_BANKING 2026-11-20): the court swaps the catalog modules with the
// largest price gap to their cheapest compatible working product until the auction proceeds cover the debt at that bank.
// Uses the refit layer (per-instance configuration), so the ship stays flyable; nothing seizable -> left alone.
// F7 apply costs money (2026-11-20): the preview's labour + materials, paid at a berth (a repair may draw the lifeline
// credit - the minimum repair is always payable, ECONOMY_BANKING §4; a replacement is a fee). DEV off a berth: free.
// "" = paid (or free), otherwise why not.
std::string GameServer::chargeRefit(EntityId id, EntityId subject, const game::ship::refit::RefitOperation& op)
{
    const Ship* commander = m_simulation.getShip(id);
    const Ship* target = m_simulation.getShip(subject.value == 0 ? id : subject);
    if (!commander || !target || !m_economy.loaded())
        return {};
    const std::uint64_t player = m_players.playerForShip(commander->core().registry().instanceId).value;
    if (player == 0)
        return {};
    const auto preview = game::ship::refit::previewRefit(target->core(), op);
    if (!preview.ok)
        return preview.error.empty() ? std::string("операция невозможна") : preview.error;
    bool tender = false;
    const std::string hub = m_simulation.serviceStationOf(id, &tender);
    if (hub.empty() || tender)
    {
        if (g_devCommandsAllowed.load())
        {
            std::cerr << "[Refit] DEV: free apply off a berth (" << preview.cost.totalCr << " Cr not charged)" << std::endl;
            return {};
        }
        return "ремонт и замена — у причала хаба";
    }
    const int system = commander->core().transform().motion.systemId;
    const auto kind = op.kind == game::ship::refit::RefitOperationKind::Repair ? game::economy::Purpose::Repair : game::economy::Purpose::Fee;
    const std::string memo = std::string(op.kind == game::ship::refit::RefitOperationKind::Repair ? "ремонт " : "замена ") + op.moduleId +
                             (op.productId.empty() ? std::string() : " -> " + op.productId);
    const auto paid = m_economy.chargeService(player, hub, system, kind, preview.cost.totalCr, memo);
    if (!paid.ok)
        return "не хватает денег: " + std::to_string(static_cast<long long>(preview.cost.totalCr)) + " Cr (" + paid.reason + ")";
    std::cerr << "[Refit] player " << player << " paid " << preview.cost.totalCr << " Cr at " << hub << " for " << memo << std::endl;
    return {};
}

void GameServer::arrestShip(EntityId id, std::uint64_t player, const std::string& hubId, const game::economy::EconomyServer::ArrestClaim& claim)
{
    Ship* ship = m_simulation.getShip(id);
    if (!ship)
        return;
    const auto seizure = game::ship::refit::exchangeSeizure(ship->core(), claim.debtCr, m_economy.auctionShare());
    const double proceeds = seizure.proceedsCr;
    const std::vector<std::string>& seized = seizure.seized;
    m_economy.settleArrest(player, claim.bankId, hubId, proceeds, seized);
}

void GameServer::handleEconomyCommand(EntityId id, const game::network::ClientEconomyCommand& command)
{
    using Op = game::network::ClientEconomyCommand::Op;
    Ship* ship = m_simulation.getShip(id);
    if (!ship || !m_economy.loaded())
        return;
    const std::uint64_t shipKey = ship->core().registry().instanceId;
    const std::uint64_t player = m_players.playerForShip(shipKey).value;
    if (player == 0)
        return;
    const auto& motion = ship->core().transform().motion;
    std::cerr << "[Trade] command op " << static_cast<int>(command.op) << " request " << command.requestId << " ship " << id.value << " "
              << command.goodId << " " << command.quantity << " t at '" << command.hubId << "'" << std::endl;
    if (command.op == Op::DevCargoAdd || command.op == Op::DevCargoClear)
    {
        if (!devCommandAccepted("cargo", id))
            return;
        if (command.op == Op::DevCargoClear)
            m_economy.devClearCargo(shipKey);
        else if (const std::string why = [&] {
                     // "<good>@<origin hub>": DEV goods loaded as if bought at another hub (trade step 4: the import duty)
                     const auto at = command.goodId.find('@');
                     return at == std::string::npos
                         ? m_economy.devAddCargo(player, shipKey, command.goodId, command.quantity, cargoCapacityOf(*ship), motion.hubId)
                         : m_economy.devAddCargo(player, shipKey, command.goodId.substr(0, at), command.quantity, cargoCapacityOf(*ship),
                                                 command.goodId.substr(at + 1));
                 }();
                 !why.empty())
            m_economy.recordTradeRefusal(player, command.requestId, "no_space", why);
        applyCargoLoad(id);
        return;
    }
    if (command.op == Op::DevSpendAll)
    {
        if (!devCommandAccepted("spend all", id))
            return;
        std::cerr << "[Trade] DEV spend all: player " << player << " paid " << m_economy.devSpendAll(player, motion.hubId, motion.systemId)
                  << " Cr" << std::endl;
        return;
    }
    if (command.op == Op::SetMineSale)   // part 3: anywhere (the order goes by radio)
    {
        m_economy.setMineSale(player, command.goodId, command.quantity > 0.5);
        return;
    }
    if (command.op == Op::LoadFromMine)   // part 3: at the mine's hub
    {
        (void)m_economy.loadFromMine(player, command.goodId, shipKey, cargoCapacityOf(*ship), motion.hubId, std::max(0.0, command.quantity),
                                     command.requestId);
        return;
    }
    if (command.op == Op::LeaseSite || command.op == Op::SignConstruction)   // step 16: at the hub (docked or in its frame)
    {
        if (motion.hubId.empty())
        {
            m_economy.recordTradeRefusal(player, command.requestId, "not_at_hub", "договор оформляется у хаба: подлетите к нему");
            return;
        }
        if (command.op == Op::LeaseSite)
            (void)m_economy.leaseSite(player, command.goodId, motion.hubId, motion.systemId, command.requestId);
        else
            (void)m_economy.signConstruction(player, command.goodId, motion.hubId, motion.systemId, command.requestId);
        return;
    }
    if (command.op == Op::HireBot || command.op == Op::OrderBotLoop || command.op == Op::FireBot)   // step 15
    {
        if (command.op == Op::FireBot)
        {
            m_economy.fireBot(player, command.goodId);
            return;
        }
        // the bot's ship: an owned ship the player does not fly (quantity = its entity id); its hub
        const EntityId botShipId{static_cast<std::uint32_t>(std::max(0.0, command.quantity))};
        const Ship* botShip = m_simulation.getShip(botShipId);
        if (command.op == Op::HireBot)
        {
            if (!botShip || botShipId == id || botShip->core().registry().ownerActor != ship->core().registry().ownerActor)
            {
                m_economy.recordTradeRefusal(player, command.requestId, "bot_no_ship", "нужен свой корабль, которым вы сейчас не управляете");
                return;
            }
            const auto& bm = botShip->core().transform().motion;
            const std::string hub = !bm.hubId.empty() ? bm.hubId : motion.hubId;
            (void)m_economy.hireBot(player, command.goodId, hub, bm.systemId, botShip->core().registry().instanceId,
                                    botShip->core().desc().catalog.passportId.empty() ? botShip->core().visualIdentity().shipName
                                                                                     : botShip->core().desc().catalog.passportId,
                                    cargoCapacityOf(*botShip), botShip->core().effectivePhysics().massKg / 1000.0, command.requestId);
            return;
        }
        // the loop starts at the bot ship's hub
        std::string hub = motion.hubId;
        int system = motion.systemId;
        for (const auto& b : m_economy.bots())
            if (b.id == command.goodId)
                if (const auto* key = &b.shipKey; *key != 0)
                    for (const auto& [eid, sp] : m_simulation.ships())
                        if (sp && sp->core().registry().instanceId == *key && !sp->core().transform().motion.hubId.empty())
                        {
                            hub = sp->core().transform().motion.hubId;
                            system = sp->core().transform().motion.systemId;
                        }
        (void)m_economy.orderBotLoop(player, command.goodId, hub, system, command.requestId);
        return;
    }
    if (command.op == Op::TakeSos)   // F5 МЧС (step 12): anywhere - the licence and the call decide
    {
        (void)m_economy.takeSos(player, command.goodId, command.requestId);
        return;
    }
    if (command.op == Op::DevGrant)
    {
        if (!devCommandAccepted("grant", id))
            return;
        m_economy.credit(player, motion.hubId, motion.systemId, std::max(0.0, command.quantity), "DEV: начислено");
        std::cerr << "[Trade] DEV grant " << command.quantity << " Cr to player " << player << std::endl;
        return;
    }
    if (command.op == Op::DevCustoms)
    {
        if (!devCommandAccepted("customs", id))
            return;
        m_economy.setDevCustomsProbability(command.quantity);
        std::cerr << "[Smuggling] DEV customs inspection probability " << (command.quantity < 0.0 ? std::string("by the rules") : std::to_string(command.quantity))
                  << std::endl;
        return;
    }
    if (command.op == Op::DevEconomyDays)
    {
        // DEV: the board's days without waiting them out (the economy steps <= 3 days a tick toward the new day)
        if (!devCommandAccepted("economy days", id))
            return;
        int days = std::clamp(static_cast<int>(std::lround(command.quantity)), 0, 60);
        if (!command.goodId.empty())
        {
            // "await <series part>": to the next publication of the first matching series at this ship's hub (0 = one is
            // open on the board now)
            const auto& board = m_economy.contracts();
            const int today = m_economy.day();
            int best = -1;
            for (const auto& p : board.postings())
                if (p.status == "open" && p.originHub == motion.hubId && p.id.find(command.goodId) != std::string::npos)
                    best = 0;
            for (const auto& s : board.series())
                if (best != 0 && s.originHub == motion.hubId && s.id.find(command.goodId) != std::string::npos)
                {
                    int d = game::economy::nextPublishDay(s, today + 1);
                    while (d < today + 400 && !game::economy::projectBuilding(s, d - board.rules().constructionEpochDay))
                        d = game::economy::nextPublishDay(s, d + 1);
                    if (d < today + 400 && (best < 0 || d - today < best))
                        best = d - today;
                }
            days = std::max(0, best);
            std::cerr << "[Contract] DEV await '" << command.goodId << "' at " << motion.hubId << ": " << (best < 0 ? "no such series" : best == 0 ? "open now" : "published in " + std::to_string(best) + " days") << std::endl;
        }
        m_devEconomyDayOffset += days;
        std::cerr << "[Contract] DEV economy clock +" << days << " days (offset now " << m_devEconomyDayOffset << ")" << std::endl;
        return;
    }
    // a deal needs a berth at that hub (TRADE_DESIGN §7.4): docked, not a tender alongside, not in a jump
    bool tender = false;
    const std::string at = m_simulation.serviceStationOf(id, &tender);
    if (command.op == Op::TakeContract || command.op == Op::AbandonContract)
    {
        // the contract board (trade step 5): taking loads the cargo, so it needs the berth of the posting's origin
        game::economy::EconomyServer::ContractOrder order;
        order.playerId = player;
        order.shipKey = shipKey;
        order.hubId = tender ? std::string() : at;
        order.systemId = motion.systemId;
        order.contractId = command.contractId;
        order.requestId = command.requestId;
        order.capacity = cargoCapacityOf(*ship);
        if (command.op == Op::TakeContract)
            (void)m_economy.takeContract(order);
        else
            (void)m_economy.abandonContract(order);
        (void)m_economy.takeCargoChange(shipKey);
        applyCargoLoad(id);
        return;
    }
    // DEV: a licence off a berth, in the hub's frame (live checks of rescue without the slow docking queue)
    std::string licenceHub = at;
    if (command.op == Op::BuyLicence && at.empty() && g_devCommandsAllowed.load() && !motion.hubId.empty())
    {
        licenceHub = motion.hubId;
        std::cerr << "[Licence] DEV: off a berth, in the frame of " << licenceHub << std::endl;
        (void)m_economy.buyLicence(player, command.goodId, licenceHub, motion.systemId, command.requestId);
        return;
    }
    if (at.empty() || tender || (!command.hubId.empty() && command.hubId != at))
    {
        m_economy.recordTradeRefusal(player, command.requestId, "not_docked",
                                     "сделка только у причала хаба" + (command.hubId.empty() ? std::string() : " " + command.hubId));
        return;
    }
    if (command.op == Op::InsureCargo)   // TRADE step 13: the own lots aboard, at this berth
    {
        (void)m_economy.insureCargo(player, shipKey, at, motion.systemId, command.requestId);
        return;
    }
    if (command.op == Op::BuyLicence)   // TRADE step 11: from the polity of this berth's hub
    {
        (void)m_economy.buyLicence(player, command.goodId, at, motion.systemId, command.requestId);
        return;
    }
    game::economy::EconomyServer::TradeOrder order;
    order.buy = command.op == Op::Buy;
    order.black = command.op == Op::SellBlack;
    order.playerId = player;
    order.shipKey = shipKey;
    order.hubId = at;
    order.systemId = motion.systemId;
    order.goodId = command.goodId;
    order.tonnes = command.quantity;
    order.limitUnitCr = command.limitUnitCr;
    order.quoteDay = command.quoteDay;
    order.requestId = command.requestId;
    order.capacity = cargoCapacityOf(*ship);
    (void)m_economy.trade(order);
    applyCargoLoad(id);
}

game::simulation::ClientTradeSnapshot
GameServer::tradeForSession(
    game::network::ServerSessionId sessionId
) const
{
    const PlayerId player = m_sessions.player(sessionId);
    if (!player || !m_economy.loaded())
        return {};
    const Ship* ship = m_simulation.getShip(controlledEntityForSession(sessionId));
    if (!ship)
        return {};
    const auto& motion = ship->core().transform().motion;
    const std::uint64_t shipKey = ship->core().registry().instanceId;
    bool tender = false;
    const std::string at = m_simulation.serviceStationOf(controlledEntityForSession(sessionId), &tender);
    auto s = m_economy.tradeSnapshot(player.value, shipKey, motion.hubId, motion.systemId, !at.empty() && !tender);
    const auto cap = cargoCapacityOf(*ship);
    s.c20Slots = cap.c20Slots;
    s.netCapacityT = cap.netT;
    s.shipMassT = ship->core().currentMassKg() / 1000.0;
    const double chargeT = ship->core().effectivePhysics().jumpChargeCapacityT;
    s.jumpRangeLy = jumpRangeLy(chargeT, s.shipMassT);
    s.jumpRangeEmptyLy = jumpRangeLy(chargeT, s.shipMassT - s.cargoGrossT);
    return s;
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

    // The navigation travels inside a copy of m_lastSnapshot: derive it from the ship pose of that
    // publication tick, not the live one. Between publications the ship keeps moving (on a planet's
    // orbit ~26 km/s, i.e. ~500 m a tick) and the session's navigation would disagree with its own
    // replicated ship (EliteServer --self-test, 2026-11-17: 519 m).
    const ShipTransform* published = nullptr;
    for (const auto& ship : m_lastSnapshot.ships)
        if (ship.id == controlledEntityId)
        {
            published = &ship.transform;
            break;
        }
    outNavigation = navigationStateForEntity(controlledEntityId, published);
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
    outSnapshot.session.wallet = walletForSession(sessionId);
    outSnapshot.session.controlledEntityId = controlledEntityForSession(sessionId).value;
    outSnapshot.session.controlEpoch = m_controlEpochs.count(playerForSession(sessionId).value) ? m_controlEpochs.at(playerForSession(sessionId).value) : 0;
    outSnapshot.session.trade = tradeForSession(sessionId);

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
    outSnapshot.session.wallet = walletForSession(sessionId);
    outSnapshot.session.controlledEntityId = controlledEntityForSession(sessionId).value;
    outSnapshot.session.controlEpoch = m_controlEpochs.count(playerForSession(sessionId).value) ? m_controlEpochs.at(playerForSession(sessionId).value) : 0;
    outSnapshot.session.trade = tradeForSession(sessionId);
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
    outSnapshot.session.wallet = walletForSession(sessionId);
    outSnapshot.session.controlledEntityId = controlledEntityForSession(sessionId).value;
    outSnapshot.session.controlEpoch = m_controlEpochs.count(playerForSession(sessionId).value) ? m_controlEpochs.at(playerForSession(sessionId).value) : 0;
    outSnapshot.session.trade = tradeForSession(sessionId);
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
