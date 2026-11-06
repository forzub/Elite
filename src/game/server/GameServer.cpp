#include "src/game/navigation/route/RoutePlanner.h"
#include <thread>

#include "src/game/system_map/PlanetBodyOrientation.h"
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





struct GameServer::TerrainField
{
    std::atomic<bool> ready {false};
    world::celestial::surface::LoadedBodySurface loaded;
};

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







int GameServer::resolveSingleActiveSimulationSystemId() const
{
    int resolvedSystemId = -1;

    for (const EntityId controlledId :
         m_simulation.playerControlledShipIds())
    {
        const Ship* ship = m_simulation.getShip(controlledId);
        if (!ship)
            continue;

        const int shipSystemId =
            ship->core().transform().motion.systemId;

        // Interstellar controlled entities do not nominate a local celestial
        // runtime. Keep whichever materialized system is already active until
        // the later multi-system/interstellar runtime stage owns that domain.
        if (shipSystemId < 0)
            continue;

        if (resolvedSystemId < 0)
        {
            resolvedSystemId = shipSystemId;
            continue;
        }

        if (resolvedSystemId != shipSystemId)
        {
            // The current production simulation still materializes one local
            // celestial system at a time. Crucially, do not pick a "primary
            // player" here: keep the already-active context until a real
            // multi-system runtime can host both systems simultaneously.
            return m_simulation.activeCelestialSystemId();
        }
    }

    return resolvedSystemId >= 0
        ? resolvedSystemId
        : m_simulation.activeCelestialSystemId();
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
            body.atmosphereOuterRadiusKm * 1000.0
        );

        m_simulation.updateStaticObjectOrbitParentParameters(
            systemId,
            body.id,
            radiusMeters,
            body.gravitationalParameterM3s2
        );
    }

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
                    ship.setControlState(cmd);
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

        if (time.universeTimeSimulation)
        {
            cmdQueue.clear();
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
                    m_simulation.requestDockDeparture(id);
                    break;

                case ClientShipCommand::RequestOrbitalTransfer:
                    m_simulation.requestOrbitalTransfer(id, shipCmd.target);
                    break;

                case ClientShipCommand::CancelOrbitalTransfer:
                    m_simulation.cancelOrbitalTransfer(id);
                    break;

                case ClientShipCommand::ToggleStabilization:
                    m_simulation.toggleStabilization(id);
                    break;

                case ClientShipCommand::ToggleJEngine:
                    m_simulation.toggleJEngine(id);
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
                    else if (shipCmd.index == 3 || shipCmd.index == 4)
                        m_simulation.cycleCombatWeapon(id, shipCmd.index == 4);
                    else if (shipCmd.index == 5)
                        m_simulation.spawnCombatTarget(id, shipCmd.amount > 0.0 ? shipCmd.amount : 400.0);
                    break;

                case ClientShipCommand::RouteLeg:
                {
                    GameSimulation::FreeFlightLeg leg;
                    const std::string& t = shipCmd.target;
                    if (t.rfind("hub:", 0) == 0)
                    {
                        leg.kind = GameSimulation::FreeFlightLeg::Kind::Hub;
                        leg.hubId = t.substr(4);
                    }
                    else if (t.rfind("orbit:", 0) == 0)
                    {
                        leg.kind = GameSimulation::FreeFlightLeg::Kind::Orbit;
                        const std::string rest = t.substr(6);
                        const auto colon = rest.rfind(':');
                        leg.bodyId = rest.substr(0, colon);
                        leg.orbitAltitudeM = colon == std::string::npos ? 100000.0 : std::atof(rest.c_str() + colon + 1) * 1000.0;
                    }
                    if (!shipCmd.detail.empty())
                    {
                        // a planned route: legs separated by '|'
                        std::vector<GameSimulation::FreeFlightLeg> legs;
                        const auto points = game::navigation::route::decodeRoute(shipCmd.detail);
                        double remaining = 0.0;
                        for (std::size_t k = 0; k < points.size(); ++k)
                        {
                            const auto& p = points[k];
                            GameSimulation::FreeFlightLeg l;
                            l.kind = p.kind == game::navigation::route::RoutePoint::Kind::Hub     ? GameSimulation::FreeFlightLeg::Kind::Hub
                                   : p.kind == game::navigation::route::RoutePoint::Kind::Orbit ? GameSimulation::FreeFlightLeg::Kind::Orbit
                                   : p.kind == game::navigation::route::RoutePoint::Kind::Land  ? GameSimulation::FreeFlightLeg::Kind::Land
                                   : p.kind == game::navigation::route::RoutePoint::Kind::Ship  ? GameSimulation::FreeFlightLeg::Kind::Ship
                                                                                                  : GameSimulation::FreeFlightLeg::Kind::Point;
                            l.landingSpec = p.landingSpec;
                            l.shipEntityId = p.shipEntityId;
                            l.hubId = p.hubId;
                            l.bodyId = p.bodyId;
                            l.orbitAltitudeM = p.orbitAltitudeM;
                            l.pointWorld = p.worldM;
                            l.passThrough = p.kind == game::navigation::route::RoutePoint::Kind::Point && !p.stop;
                            l.action = p.action;
                            legs.push_back(l);
                        }
                        // the rest of the route after each pass-through point (straight-line
                        // estimate on the server's own positions: hubs / bodies are sent by id)
                        std::vector<glm::dvec3> at(points.size());
                        for (std::size_t k = 0; k < points.size(); ++k)
                            at[k] = m_simulation.routePointWorld(legs[k]);
                        for (std::size_t k = legs.size(); k-- > 1;)
                        {
                            remaining += glm::length(at[k] - at[k - 1]);
                            legs[k - 1].remainingAfterM = remaining;
                            if (!legs[k - 1].passThrough)
                                remaining = 0.0;
                        }
                        m_simulation.startRoute(id, std::move(legs));
                    }
                    else if (shipCmd.index == 0)
                        m_simulation.cancelRoute(id);
                    else if (shipCmd.index == 3)
                        m_simulation.pauseRoute(id);
                    else if (shipCmd.index == 4)
                        m_simulation.resumeRoute(id);
                    else
                        m_simulation.startFreeFlightLeg(id, leg);
                    break;
                }

                case ClientShipCommand::FreeFlightAutopilot:
                    m_simulation.setFreeFlightAutopilot(id, shipCmd.target, shipCmd.index != 0, shipCmd.detail);
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

// The materialized celestial context is a world-runtime concern, not a
// per-session navigation value. If every connected human ship currently names
// the same local system, that system may nominate the single materialized
// context. Split-system play remains explicitly deferred to multi-system
// runtime; no arbitrary "primary player" is allowed to choose the context.
const int simulationContextSystemId =
    resolveSingleActiveSimulationSystemId();

if (simulationContextSystemId >= 0)
{
    std::unordered_map<std::string, glm::dvec3> celestialPositionsAu;
    std::unordered_map<std::string, glm::dvec3>
        celestialVelocitiesAuPerSecond;

    if (const auto* celestial =
            celestialSnapshotForSystem(simulationContextSystemId))
    {
        for (const auto& state : celestial->bodies)
        {
            celestialPositionsAu[state.id] = state.positionAu;
            celestialVelocitiesAuPerSecond[state.id] =
                state.velocityAuPerSecond;
        }
    }

    m_simulation.setCelestialBodyKinematicStateAu(
        simulationContextSystemId,
        celestialPositionsAu,
        celestialVelocitiesAuPerSecond
    );

    if (m_appliedSimulationContextSystemId !=
        simulationContextSystemId)
    {
        applyCelestialOrbitParentParameters(simulationContextSystemId);
    }
}

m_simulation.update(time);

// Navigation sensors observe the completed authoritative epoch. Their cadence
// is device-owned and uses universe time; render/snapshot FPS cannot create a
// measurement. The synthetic radar asks for truth rows only when a scan is due.
updateNavigationSensorDevices(universeTime);











    if (m_forceSnapshotPublication ||
        m_serverTick % m_snapshotInterval == 0)
    {
        // Snapshot construction belongs to the publication cadence. The
        // simulation step above mutates authoritative state only; replication
        // DTOs are materialized here when they can actually be delivered.
        m_lastSnapshot =
            m_simulation.buildReplicationSnapshot(m_serverTick);
        populateClientSessionSnapshot(m_lastSnapshot);
        m_canonicalReplicationSnapshot =
            game::network::materializeCanonicalReplicationSnapshot(
                &m_canonicalReplicationSnapshot,
                m_lastSnapshot
            );
        m_forceSnapshotPublication = false;
    }

    processPendingMapRequests();
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
    outSnapshot.replication.entitySetMode =
        game::network::ReplicatedEntitySetMode::SparseRetainMissing;
    outSnapshot.replication.removedShipIds = selection.removedShipIds;
    outSnapshot.replication.removedObjectIds = selection.removedObjectIds;
    outSnapshot.replication.removedHubIds = selection.removedHubIds;

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
    const world::celestial::CelestialBodyState* body = nullptr;
    for (const auto& b : snapshot->bodies)
        if (b.id == bodyId)
            body = &b;
    if (!body || body->sourcePath.empty())
        return false;

    auto& slot = m_terrainFields[bodyId];
    if (!slot)
    {
        // the same source the flight renderer loads: curated maps or the generated body
        slot = std::make_shared<TerrainField>();
        world::celestial::surface::BodySurfaceSource source;
        source.id = bodyId;
        source.descriptorPath = body->sourcePath;
        source.radiusM = body->radiusKm * 1000.0;
        world::celestial::surface::findImportedBodyMaps(".", snapshot->systemName, body->sourcePath, source);
        std::thread([field = slot, source] {
            world::celestial::surface::loadBodySurface(source, field->loaded);
            field->ready.store(true, std::memory_order_release);
            std::cerr << "[Terrain] " << source.id << ": surface field "
                      << (field->loaded.field ? "loaded" : "FAILED " + field->loaded.error)
                      << (source.importedHeightPath.empty() ? " (procedural)" : " (curated height map)") << std::endl;
        }).detach();
        return false;
    }
    if (!slot->ready.load(std::memory_order_acquire) || !slot->loaded.field)
        return false;

    // world -> body-fixed direction with the renderer's orientation (prime, north, east)
    const auto o = game::system_map::makePlanetTextureOrientation(
        body->axialTiltDeg, body->axisNodeDeg, body->rotationPhaseRad, body->textureLongitudeOffsetDeg);
    const glm::dvec3 d = glm::normalize(worldDirection);
    const glm::dvec3 dirBody(glm::dot(d, glm::dvec3(o.prime)), glm::dot(d, glm::dvec3(o.north)), glm::dot(d, glm::dvec3(o.east)));
    const auto& field = *slot->loaded.field;
    const glm::dvec3 n = glm::normalize(dirBody);
    // the ground the ship stands on is the RENDERED one: the finest terrain level near the ground
    // shows the field down to ~1 m (small craters, rims, ejecta - CraterLayer to 1.5 m); a 30 m
    // sample smoothed them away and the hull sank into the rendered relief (author 2026-11-06).
    // The slope and the appearance stay on the 30 m relief (a stable, noise-free gradient).
    const auto sample = field.sample(n, 30.0);
    const auto detail = field.sample(n, 1.0);
    // the sea is solid too: its surface over a sea-floor height
    const bool water = sample.water && sample.waterLevelM >= sample.heightM;
    ground.heightM = water ? sample.waterLevelM : detail.heightM;
    ground.smoothHeightM = water ? sample.waterLevelM : sample.heightM;
    // slope under the hull: central differences +-15 m (a 30 m base, the hull's scale). A one-sided
    // 60 m step reached past a settlement pad (flat to 36 m) onto the slope beyond it - a perfect
    // landing on the Shackleton pad read 32.6 deg and counted as tipping over (author 2026-11-06).
    const double radius = std::max(1.0, field.referenceRadiusM());
    constexpr double HalfBaseM = 15.0;
    const double step = HalfBaseM / radius;
    glm::dvec3 t1 = glm::normalize(glm::cross(n, std::abs(n.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0)));
    const glm::dvec3 t2 = glm::cross(n, t1);
    const auto at = [&](const glm::dvec3& t, double s) { return field.sample(glm::normalize(n + t * (s * step)), 30.0).heightM; };
    const double g1 = (at(t1, 1.0) - at(t1, -1.0)) / (2.0 * HalfBaseM);
    const double g2 = (at(t2, 1.0) - at(t2, -1.0)) / (2.0 * HalfBaseM);
    const double gradient = std::sqrt(g1 * g1 + g2 * g2);
    ground.slopeDeg = water ? 0.0 : glm::degrees(std::atan(gradient));
    // the dominant material of the rendered surface there
    if (water)
        ground.material = GameSimulation::TerrainWater;
    else
    {
        const auto look = field.appearance(sample, n, gradient);
        int best = 0;
        for (int k = 1; k < static_cast<int>(look.weights.size()); ++k)
            if (look.weights[static_cast<std::size_t>(k)] > look.weights[static_cast<std::size_t>(best)])
                best = k;
        ground.material = best;
    }
    ground.feature = static_cast<int>(sample.featureClass);
    return true;
}
