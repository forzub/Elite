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

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/world_state/InitialWorldState.h"
#include "src/game/navigation/GalaxyNavigationConfig.h"
#include "src/game/navigation/PlayerSpatialDomainResolver.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/navigation/AcceptedManeuverProgramBuilder.h"
#include "src/game/navigation/DockingAdvisoryPlanner.h"
#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/DockingCompatibility.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/HubNavigationClearancePolicy.h"
#include "src/game/navigation/NavigationFrameBoundary.h"
#include "src/game/navigation/ManeuverProgramTimeline.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationVehicleProfileAdapters.h"
#include "src/world/navigation/NavigationObstacleFactory.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/world/navigation/TrajectoryGenerator.h"
#include "src/world/coordinates/WorldPosition.h"



namespace {










































}


GameServer::GameServer(std::size_t bootstrapPlayerSlotCount)
    : m_diagnostics{}
    , m_simulation(m_diagnostics)
{

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

        if (!m_serverHubSemanticAnchorCatalog.load())
        {
            std::cerr
                << "[GameServer] hub semantic anchor catalog was not loaded\n";
        }

        if (!m_serverDockingPortRuntimeStateCatalog.load())
        {
            std::cerr
                << "[GameServer] docking runtime catalog was not loaded\n";
        }

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
            body.gravitationalParameterM3s2
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
















void GameServer::update(double dt)
{
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

                default:
                    ship.applyCommand(shipCmd);
                    break;
            }

            cmdQueue.pop_front();
        }
    }



    // Human inputs remain numbered/acknowledged, but docking Autopilot
    // owns the physical control sample while preparation or accepted automatic
    // execution is active. Automatic execution always wins through the
    // AcceptedManeuverProgram -> Follower -> bridge seam.
    applyDockingGuidancePreparationControls();
    applyAutomaticDockingControls(time);

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

bool GameServer::beginDockingGuidancePreparation(
    PlayerId playerId,
    EntityId controlledEntityId,
    std::uint64_t requestSerial
)
{
    if (!playerId || controlledEntityId.value == 0 || requestSerial == 0)
        return false;

    Ship* ship = m_simulation.getShip(controlledEntityId);
    if (!ship)
        return false;

    if (const auto automatic =
            m_dockingAutomaticRuntimes.find(controlledEntityId.value);
        automatic != m_dockingAutomaticRuntimes.end())
    {
        const PlayerId automaticPlayer = automatic->second.playerId;
        const std::uint64_t automaticSerial =
            automatic->second.requestSerial;
        (void)finishAutomaticDocking(
            automaticPlayer,
            controlledEntityId,
            automaticSerial,
            false,
            "manual-guidance-request"
        );
    }

    const auto& transform = ship->core().transform();
    const auto& motion = transform.motion;
    const std::string hubId = !motion.matchedReferenceFrameId.empty()
        ? motion.matchedReferenceFrameId : motion.hubId;
    const auto* hub = hubId.empty() ? nullptr : m_simulation.hubNavigationFrame(hubId);
    if (!hub || !hub->valid || hub->systemId != motion.systemId ||
        !motion.matchedToReferenceFrame ||
        motion.matchedReferenceFrameId != hubId)
    {
        std::cerr << "[DockPrep] rejected entity=" << controlledEntityId.value
                  << " request=" << requestSerial
                  << " reason=ship-not-matched-to-hub\n";
        return false;
    }

    const auto existing = m_dockingGuidancePreparations.find(controlledEntityId.value);
    if (existing != m_dockingGuidancePreparations.end())
    {
        if (existing->second.playerId == playerId &&
            existing->second.requestSerial == requestSerial)
            return true;
        const auto oldPrep = existing->second;
        (void)finishDockingGuidancePreparation(
            oldPrep.playerId, oldPrep.entityId, oldPrep.requestSerial, false);
    }

    if (!m_controls.takeAutopilotControl(playerId, controlledEntityId))
        return false;

    if (auto it = m_controlStreams.find(controlledEntityId.value);
        it != m_controlStreams.end())
        it->second.discardPendingAndAcknowledgeNewest();
    m_pendingClientShipCommands.erase(controlledEntityId.value);

    DockingGuidancePreparation prep;
    prep.requestSerial = requestSerial;
    prep.playerId = playerId;
    prep.entityId = controlledEntityId;
    prep.hubId = hubId;
    m_dockingGuidancePreparations[controlledEntityId.value] = std::move(prep);

    ShipControlState stop;
    stop.velocityAlignmentCommand =
        game::navigation::VelocityAlignmentMode::BrakeToStop;
    ship->setControlState(stop);
    m_forceSnapshotPublication = true;

    const double initialAngularRateRadPerSec = std::sqrt(
        static_cast<double>(transform.pitchRate) *
            static_cast<double>(transform.pitchRate) +
        static_cast<double>(transform.yawRate) *
            static_cast<double>(transform.yawRate) +
        static_cast<double>(transform.rollRate) *
            static_cast<double>(transform.rollRate)
    );
    const ShipParams effectivePhysics = ship->core().effectivePhysics();
    const double forwardMainBrakingAuthorityMps2 =
        game::ship::forwardMainAccelerationLimitMps2(effectivePhysics);
    const double reverseMainBrakingAuthorityMps2 =
        game::ship::reverseMainAccelerationLimitMps2(effectivePhysics);

    std::cout << "[DockPrep] begin entity=" << controlledEntityId.value
              << " request=" << requestSerial
              << " hub=" << hubId
              << " vrel_mps=" << glm::length(motion.localVelocityMps)
              << " omega_radps=" << initialAngularRateRadPerSec
              << " law="
              << game::navigation::localFlightControlLawName(
                     motion.localControlLaw
                 )
              << " forward_main_mps2="
              << forwardMainBrakingAuthorityMps2
              << " reverse_main_mps2="
              << reverseMainBrakingAuthorityMps2
              << '\n';
    return true;
}

bool GameServer::finishDockingGuidancePreparation(
    PlayerId playerId,
    EntityId controlledEntityId,
    std::uint64_t requestSerial,
    bool routePublished
)
{
    const auto it = m_dockingGuidancePreparations.find(controlledEntityId.value);
    if (it == m_dockingGuidancePreparations.end())
        return false;
    const auto prep = it->second;
    if (prep.playerId != playerId || prep.requestSerial != requestSerial)
        return false;

    if (auto streamIt = m_controlStreams.find(controlledEntityId.value);
        streamIt != m_controlStreams.end())
        streamIt->second.discardPendingAndAcknowledgeNewest();

    if (Ship* ship = m_simulation.getShip(controlledEntityId))
        ship->setControlState(ShipControlState {});

    const bool restored = m_controls.restoreHumanControl(
        playerId, controlledEntityId);
    m_dockingGuidancePreparations.erase(it);
    m_forceSnapshotPublication = true;

    std::cout << "[DockPrep] "
              << (routePublished ? "published" : "cancelled")
              << " entity=" << controlledEntityId.value
              << " request=" << requestSerial
              << " human_restored=" << (restored ? 1 : 0) << '\n';
    return restored;
}

void GameServer::applyDockingGuidancePreparationControls()
{
    std::vector<DockingGuidancePreparation> invalid;
    invalid.reserve(m_dockingGuidancePreparations.size());

    for (const auto& [entityValue, prep] : m_dockingGuidancePreparations)
    {
        Ship* ship = m_simulation.getShip(prep.entityId);
        if (!ship)
        {
            invalid.push_back(prep);
            continue;
        }

        const auto& motion = ship->core().transform().motion;
        const auto* hub = m_simulation.hubNavigationFrame(prep.hubId);
        if (!hub || !hub->valid || hub->systemId != motion.systemId ||
            !motion.matchedToReferenceFrame ||
            motion.matchedReferenceFrameId != prep.hubId ||
            m_controls.controllerKind(prep.entityId) !=
                game::server::ControllerKind::Autopilot)
        {
            invalid.push_back(prep);
            continue;
        }

        if (auto streamIt = m_controlStreams.find(entityValue);
            streamIt != m_controlStreams.end())
            streamIt->second.discardPendingAndAcknowledgeNewest();

        // Reuse the established physical END/autobrake primitive. Newtonian
        // rotates and uses installed main thrust; Assisted uses installed
        // propulsion. Neutral attitude axes retain bounded angular damping.
        ShipControlState stop;
        stop.velocityAlignmentCommand =
            game::navigation::VelocityAlignmentMode::BrakeToStop;
        ship->setControlState(stop);
    }

    for (const auto& prep : invalid)
        (void)finishDockingGuidancePreparation(
            prep.playerId, prep.entityId, prep.requestSerial, false);
}



bool GameServer::beginAutomaticDocking(
    PlayerId playerId,
    EntityId controlledEntityId,
    const ClientShipCommand& command
)
{
    if (!playerId ||
        controlledEntityId.value == 0 ||
        command.requestSerial == 0 ||
        command.dockingTargetSystemId < 0 ||
        command.dockingTargetModuleId.empty() ||
        command.dockingTargetAnchorId.empty())
    {
        return false;
    }

    Ship* ship = m_simulation.getShip(controlledEntityId);
    if (!ship)
        return false;

    const auto& motion = ship->core().transform().motion;
    const std::string hubId =
        !motion.matchedReferenceFrameId.empty()
            ? motion.matchedReferenceFrameId
            : motion.hubId;

    const auto* hub = hubId.empty()
        ? nullptr
        : m_simulation.hubNavigationFrame(hubId);

    if (!hub ||
        !hub->valid ||
        hub->systemId != command.dockingTargetSystemId ||
        motion.systemId != command.dockingTargetSystemId ||
        !motion.matchedToReferenceFrame ||
        motion.matchedReferenceFrameId != hubId)
    {
        std::cerr
            << "[DockAuto] rejected entity="
            << controlledEntityId.value
            << " request=" << command.requestSerial
            << " reason=ship-not-matched-to-target-hub\n";
        return false;
    }

    if (const auto manual =
            m_dockingGuidancePreparations.find(controlledEntityId.value);
        manual != m_dockingGuidancePreparations.end())
    {
        const auto prep = manual->second;
        (void)finishDockingGuidancePreparation(
            prep.playerId,
            prep.entityId,
            prep.requestSerial,
            false
        );
    }

    if (const auto existing =
            m_dockingAutomaticRuntimes.find(controlledEntityId.value);
        existing != m_dockingAutomaticRuntimes.end())
    {
        if (existing->second.playerId == playerId &&
            existing->second.requestSerial == command.requestSerial &&
            existing->second.targetModuleId ==
                command.dockingTargetModuleId &&
            existing->second.targetAnchorId ==
                command.dockingTargetAnchorId)
        {
            return true;
        }

        const auto oldPlayer = existing->second.playerId;
        const auto oldSerial = existing->second.requestSerial;
        (void)finishAutomaticDocking(
            oldPlayer,
            controlledEntityId,
            oldSerial,
            false,
            "superseded"
        );
    }

    if (!m_controls.takeAutopilotControl(playerId, controlledEntityId))
        return false;

    if (auto it = m_controlStreams.find(controlledEntityId.value);
        it != m_controlStreams.end())
    {
        it->second.discardPendingAndAcknowledgeNewest();
    }
    m_pendingClientShipCommands.erase(controlledEntityId.value);

    DockingAutomaticRuntime runtime;
    runtime.requestSerial = command.requestSerial;
    runtime.playerId = playerId;
    runtime.entityId = controlledEntityId;
    runtime.systemId = command.dockingTargetSystemId;
    runtime.hubId = hubId;
    runtime.targetModuleId = command.dockingTargetModuleId;
    runtime.targetAnchorId = command.dockingTargetAnchorId;
    runtime.phase = DockingAutomaticRuntime::Phase::Stabilizing;
    runtime.settledSinceUniverseTimeSeconds = -1.0;
    runtime.nextPlanAttemptUniverseTimeSeconds = 0.0;
    runtime.nextProgramRevision = 1;

    m_dockingAutomaticRuntimes[controlledEntityId.value] =
        std::move(runtime);

    ShipControlState stop;
    stop.velocityAlignmentCommand =
        game::navigation::VelocityAlignmentMode::BrakeToStop;
    ship->setControlState(stop);

    m_forceSnapshotPublication = true;
    std::cout
        << "[DockAuto] begin entity=" << controlledEntityId.value
        << " request=" << command.requestSerial
        << " hub=" << hubId
        << " target=" << command.dockingTargetModuleId
        << ":" << command.dockingTargetAnchorId
        << " phase=stabilizing\n";
    return true;
}


bool GameServer::planAutomaticDocking(
    DockingAutomaticRuntime& runtime,
    Ship& ship,
    double universeTimeSeconds
)
{
    using namespace game::navigation;

    const auto* hub =
        m_simulation.hubNavigationFrame(runtime.hubId);
    if (!hub ||
        !hub->valid ||
        hub->systemId != runtime.systemId ||
        !std::isfinite(universeTimeSeconds))
    {
        return false;
    }

    const StaticObject* targetObject = nullptr;
    for (const auto& [id, object] : m_simulation.staticObjects())
    {
        (void)id;
        if (object.systemId == runtime.systemId &&
            object.attachedToHub &&
            object.hubId == runtime.hubId &&
            object.hubModuleId == runtime.targetModuleId)
        {
            targetObject = &object;
            break;
        }
    }
    if (!targetObject)
        return false;

    const auto* definition =
        m_serverHubSemanticAnchorCatalog.find(
            runtime.targetModuleId,
            runtime.targetAnchorId
        );
    const auto* portRuntime =
        m_serverDockingPortRuntimeStateCatalog.find(
            runtime.targetModuleId,
            runtime.targetAnchorId
        );
    if (!definition ||
        !portRuntime ||
        !definition->enabled ||
        definition->kind != HubSemanticAnchorKind::DockingPort)
    {
        return false;
    }

    const auto& dimensions =
        ship.core().descriptor().logicalDimensions();

    ShipDockingEnvelope hull;
    hull.valid =
        dimensions.enabled &&
        dimensions.length > 0.0f &&
        dimensions.width > 0.0f &&
        dimensions.height > 0.0f;
    hull.lengthMeters = dimensions.length;
    hull.widthMeters = dimensions.width;
    hull.heightMeters = dimensions.height;

    const auto compatibility =
        evaluateDockingCompatibility(
            hull,
            *definition,
            *portRuntime
        );
    if (!compatibility.routeAvailable)
        return false;

    game::simulation::HubAttachmentSnapshot attachment;
    attachment.systemId = targetObject->systemId;
    attachment.hubId = targetObject->hubId;
    attachment.moduleId = targetObject->hubModuleId;
    attachment.localOffsetMeters =
        targetObject->hubLocalOffsetMeters;
    attachment.localRotationDeg =
        targetObject->hubLocalRotationDeg;
    attachment.localAngularVelocityDegPerSecond =
        targetObject->hubLocalAngularVelocityDegPerSecond;
    attachment.inheritHubOrientation =
        targetObject->inheritHubOrientation;
    attachment.valid = targetObject->attachedToHub;

    const auto portAt =
        [&](double epoch)
        {
            return resolveDockingAdvisoryLocalPortAt(
                attachment,
                *definition,
                epoch
            );
        };

    const ShipParams physics =
        ship.core().effectivePhysics();

    VehicleGuidanceEnvelope envelope;
    envelope.lengthMeters = hull.lengthMeters;
    envelope.widthMeters = hull.widthMeters;
    envelope.heightMeters = hull.heightMeters;
    envelope.valid = hull.valid;

    auto fullVehicle =
        makeNavigationVehicleProfile(
            physics,
            envelope
        );

    const double linearReserve = std::min(
        0.5,
        std::max(
            0.0,
            fullVehicle.maxLateralAccelerationMps2 * 0.20
        )
    );
    const double angularReserve = std::min(
        0.25,
        std::max(
            0.0,
            fullVehicle.maxAngularAccelerationRadPerSecond2 * 0.10
        )
    );

    if (fullVehicle.maxForwardAccelerationMps2 <= linearReserve ||
        fullVehicle.maxBrakingAccelerationMps2 <= linearReserve ||
        fullVehicle.maxLateralAccelerationMps2 <= linearReserve ||
        fullVehicle.maxAngularAccelerationRadPerSecond2 <= angularReserve)
    {
        return false;
    }

    auto executionVehicle = fullVehicle;
    executionVehicle.maxForwardAccelerationMps2 -= linearReserve;
    executionVehicle.maxBrakingAccelerationMps2 -= linearReserve;
    executionVehicle.maxLateralAccelerationMps2 -= linearReserve;
    executionVehicle.maxAngularAccelerationRadPerSecond2 -=
        angularReserve;

    const auto buildObstaclesAt =
        [&](double epoch)
        {
            std::vector<world::navigation::NavigationObstacle> obstacles;
            for (const auto& [id, object] : m_simulation.staticObjects())
            {
                if (object.systemId != runtime.systemId ||
                    !object.attachedToHub ||
                    object.hubId != runtime.hubId)
                {
                    continue;
                }

                const glm::dvec3 angles =
                    object.hubLocalRotationDeg +
                    object.hubLocalAngularVelocityDegPerSecond *
                        epoch;
                const glm::dmat3 visualBasis(
                    glm::mat3(
                        hubLocalEulerDegToMatrix(angles)
                    )
                );

                glm::dmat3 tacticalBasis(1.0);
                for (int axis = 0; axis < 3; ++axis)
                {
                    tacticalBasis[axis] =
                        hubVisualToTacticalVector(
                            glm::dvec3(visualBasis[axis])
                        );
                }

                const auto obstacle =
                    world::navigation::makeNavigationObstacleForObject(
                        object.type,
                        "object:" + std::to_string(id.value),
                        id.value,
                        hubVisualToTacticalVector(
                            object.hubLocalOffsetMeters
                        ),
                        tacticalBasis,
                        game::navigation::
                            DiagnosticHubInfrastructureClearanceMeters
                    );
                if (obstacle)
                    obstacles.push_back(*obstacle);
            }
            return obstacles;
        };

    const auto& motion = ship.core().transform().motion;
    const bool assisted =
        motion.localControlLaw ==
            game::navigation::LocalFlightControlLaw::Assisted;

    world::navigation::TrajectoryGenerationResult trajectoryResult;
    DockingAdvisoryPlan advisoryPlan;
    double captureUniverseTimeSeconds = universeTimeSeconds;
    double finalPreCaptureDepthMeters = 0.0;
    glm::dvec3 terminalAngularVelocityMapRadPerSec(0.0);

    for (int iteration = 0; iteration < 3; ++iteration)
    {
        const auto port =
            portAt(captureUniverseTimeSeconds);
        if (!port.valid)
            return false;

        DockingAdvisoryRequest request;
        request.startMeters = motion.localPositionMeters;
        request.entranceMeters = port.positionMeters;
        request.outward = port.forward;
        request.standoffMeters = std::max(
            300.0,
            hull.lengthMeters * 10.0 +
                definition->requiredClearanceMeters * 4.0
        );
        request.hullRadiusMeters =
            envelope.conservativeSafetyRadiusMeters();
        request.maxSpeedMps =
            executionVehicle.maxSpeedMps;
        request.brakingMps2 =
            executionVehicle.maxBrakingAccelerationMps2;
        request.lateralMps2 =
            executionVehicle.maxLateralAccelerationMps2;
        request.gateSpacingMeters = 500.0;
        request.terminalGateSpacingMeters = 250.0;
        request.terminalDenseDistanceMeters = 2000.0;
        if (assisted)
        {
            request.terminalApproachLengthMeters = 9000.0;
            request.terminalTurnSegmentFraction = 0.85;
            request.preferredTerminalTurnRadiusMeters = 6000.0;
        }
        request.obstacles =
            buildObstaclesAt(captureUniverseTimeSeconds);

        advisoryPlan =
            DockingAdvisoryPlanner::plan(request);
        if (!advisoryPlan.valid())
            return false;

        world::navigation::TrajectoryGenerationRequest trajectoryRequest;
        trajectoryRequest.systemId = runtime.systemId;
        trajectoryRequest.frameId = runtime.hubId;
        trajectoryRequest.startUniverseTimeSeconds =
            universeTimeSeconds;
        trajectoryRequest.universeTimeScale = 1.0;
        trajectoryRequest.obstacles = request.obstacles;
        trajectoryRequest.vehicle = executionVehicle;
        trajectoryRequest.initialVelocityMps =
            motion.localVelocityMps;
        trajectoryRequest.initialAccelerationMps2 =
            glm::dvec3(0.0);

        trajectoryRequest.pathPointsMeters.reserve(
            advisoryPlan.gates.size() + 1
        );
        for (const auto& gate : advisoryPlan.gates)
        {
            trajectoryRequest.pathPointsMeters.push_back(
                gate.positionMeters
            );
        }

        if (trajectoryRequest.pathPointsMeters.empty())
            return false;

        const glm::dvec3 advisoryStopMeters =
            trajectoryRequest.pathPointsMeters.back();

        // The target module is still authoritative solid collision geometry.
        // Automatic navigation therefore terminates this execution slice
        // OUTSIDE that geometry. Never use TrajectoryGenerator's
        // terminalAllowedObstacleId as a planner-only collision exemption:
        // shared physics would still collide with the same module.
        const double minimumPreCaptureDepthMeters =
            request.hullRadiusMeters +
            game::navigation::
                DiagnosticHubInfrastructureClearanceMeters +
            game::navigation::
                AutomaticDockingPreCaptureReserveMeters;

        double preCaptureDepthMeters = std::min(
            request.standoffMeters,
            minimumPreCaptureDepthMeters
        );

        const auto preCaptureCenterAt =
            [&](const DockingAdvisoryLocalPort& portState,
                double depthMeters)
            {
                return
                    portState.positionMeters +
                    portState.forward * depthMeters;
            };

        const auto segmentToPreCaptureClear =
            [&](double depthMeters)
            {
                return world::navigation::
                    segmentClearOfNavigationObstacles(
                        advisoryStopMeters,
                        preCaptureCenterAt(port, depthMeters),
                        request.obstacles,
                        request.hullRadiusMeters
                    );
            };

        if (!segmentToPreCaptureClear(preCaptureDepthMeters))
        {
            // Back away toward the already-proved advisory stop until the
            // complete conservative swept-sphere chord is collision-free.
            double blockedDepthMeters = preCaptureDepthMeters;
            double safeDepthMeters = request.standoffMeters;

            if (!segmentToPreCaptureClear(safeDepthMeters))
                return false;

            for (int search = 0; search < 24; ++search)
            {
                const double probeDepthMeters =
                    0.5 * (
                        blockedDepthMeters +
                        safeDepthMeters
                    );

                if (segmentToPreCaptureClear(probeDepthMeters))
                    safeDepthMeters = probeDepthMeters;
                else
                    blockedDepthMeters = probeDepthMeters;
            }

            preCaptureDepthMeters =
                safeDepthMeters +
                game::navigation::
                    AutomaticDockingPreCaptureReserveMeters;
            preCaptureDepthMeters = std::min(
                request.standoffMeters,
                preCaptureDepthMeters
            );

            if (!segmentToPreCaptureClear(preCaptureDepthMeters))
                return false;
        }

        const glm::dvec3 preCaptureCenterMeters =
            preCaptureCenterAt(
                port,
                preCaptureDepthMeters
            );
        finalPreCaptureDepthMeters =
            preCaptureDepthMeters;

        if (glm::length(
                trajectoryRequest.pathPointsMeters.back() -
                preCaptureCenterMeters) > 1.0e-6)
        {
            trajectoryRequest.pathPointsMeters.push_back(
                preCaptureCenterMeters
            );
        }

        // Preserve the planner's gate speed doctrine and extend it only to a
        // collision-free pre-capture center pose. Physical contact/latch is a
        // separate authority transition and is not faked by navigation.
        double sourceProgress = 0.0;
        for (std::size_t i = 0;
             i < advisoryPlan.gates.size();
             ++i)
        {
            if (i > 0)
            {
                sourceProgress += glm::length(
                    advisoryPlan.gates[i].positionMeters -
                    advisoryPlan.gates[i - 1].positionMeters
                );
            }

            world::navigation::TrajectoryPointSpeedConstraint constraint;
            constraint.sourcePathProgressMeters = sourceProgress;
            if (i + 1 == advisoryPlan.gates.size())
            {
                const double authoredEntry =
                    definition->maxEntrySpeedMps > 0.0
                        ? definition->maxEntrySpeedMps
                        : 2.0;
                constraint.maxSpeedMps = std::min(
                    executionVehicle.maxSpeedMps,
                    std::max(0.5, authoredEntry)
                );
            }
            else
            {
                constraint.maxSpeedMps = std::min(
                    executionVehicle.maxSpeedMps,
                    std::max(
                        0.5,
                        advisoryPlan.gates[i].speedMps
                    )
                );
            }
            trajectoryRequest.pointSpeedConstraints.push_back(
                constraint
            );
        }

        if (trajectoryRequest.pathPointsMeters.size() >
            advisoryPlan.gates.size())
        {
            const double terminalProgressMeters =
                sourceProgress +
                glm::length(
                    preCaptureCenterMeters -
                    advisoryStopMeters
                );

            world::navigation::TrajectoryPointSpeedConstraint terminal;
            terminal.sourcePathProgressMeters =
                terminalProgressMeters;
            const double authoredEntry =
                definition->maxEntrySpeedMps > 0.0
                    ? definition->maxEntrySpeedMps
                    : 2.0;
            terminal.maxSpeedMps = std::min(
                executionVehicle.maxSpeedMps,
                std::max(0.5, authoredEntry)
            );
            trajectoryRequest.pointSpeedConstraints.push_back(
                terminal
            );
        }

        const double velocityProbeSeconds = 0.01;
        const auto portBefore = portAt(
            captureUniverseTimeSeconds - velocityProbeSeconds
        );
        const auto portAfter = portAt(
            captureUniverseTimeSeconds + velocityProbeSeconds
        );
        if (!portBefore.valid || !portAfter.valid)
            return false;

        const glm::dvec3 preCaptureBefore =
            preCaptureCenterAt(
                portBefore,
                preCaptureDepthMeters
            );
        const glm::dvec3 preCaptureAfter =
            preCaptureCenterAt(
                portAfter,
                preCaptureDepthMeters
            );

        trajectoryRequest.hasTerminalVelocity = true;
        trajectoryRequest.terminalVelocityMps =
            (preCaptureAfter -
             preCaptureBefore) /
            (2.0 * velocityProbeSeconds);

        const auto portOrientation =
            [](const DockingAdvisoryLocalPort& value)
            {
                const glm::dvec3 right = glm::normalize(
                    glm::cross(value.forward, value.up)
                );
                const glm::dvec3 up = glm::normalize(
                    glm::cross(right, value.forward)
                );
                return glm::normalize(
                    glm::quat_cast(
                        glm::dmat3(
                            right,
                            up,
                            -value.forward
                        )
                    )
                );
            };

        glm::dquat rotationDelta = glm::normalize(
            portOrientation(portAfter) *
            glm::conjugate(portOrientation(portBefore))
        );
        if (rotationDelta.w < 0.0)
            rotationDelta = -rotationDelta;

        const double deltaW =
            std::clamp(rotationDelta.w, -1.0, 1.0);
        const double deltaAngle =
            2.0 * std::acos(deltaW);
        const double deltaSinHalf =
            std::sqrt(std::max(
                0.0,
                1.0 - deltaW * deltaW
            ));

        terminalAngularVelocityMapRadPerSec =
            glm::dvec3(0.0);
        if (deltaAngle > 1.0e-9 &&
            deltaSinHalf > 1.0e-9)
        {
            terminalAngularVelocityMapRadPerSec =
                glm::dvec3(
                    rotationDelta.x / deltaSinHalf,
                    rotationDelta.y / deltaSinHalf,
                    rotationDelta.z / deltaSinHalf
                ) *
                (deltaAngle /
                 (2.0 * velocityProbeSeconds));
        }

        trajectoryRequest.hasTerminalOrientation = true;
        trajectoryRequest.terminalForward =
            -port.forward;
        trajectoryRequest.terminalUp =
            port.up;
        trajectoryRequest.terminalOrientationBlendDistanceMeters =
            std::max(500.0, request.standoffMeters * 2.0);

        trajectoryResult =
            world::navigation::TrajectoryGenerator::generate(
                trajectoryRequest
            );
        if (!trajectoryResult.ready())
            return false;

        const double predictedCapture =
            universeTimeSeconds +
            trajectoryResult.trajectory.durationSeconds;

        if (std::abs(
                predictedCapture -
                captureUniverseTimeSeconds) <= 0.05)
        {
            captureUniverseTimeSeconds = predictedCapture;
            break;
        }

        captureUniverseTimeSeconds = predictedCapture;
    }

    if (!trajectoryResult.ready())
        return false;

    AcceptedManeuverProgramBuilder::Request build;
    build.trajectory = &trajectoryResult.trajectory;
    build.shipPhysics = &physics;
    build.objectiveRevision = runtime.requestSerial;
    build.firstProgramRevision = runtime.nextProgramRevision;
    build.capabilityRevision = runtime.requestSerial;
    build.mapRevision = m_serverTick;
    build.mapSourceRevision = m_serverTick;
    build.spaceRevision = m_serverTick;
    build.spaceSourceRevision = m_serverTick;
    build.minimumClearanceMeters = 0.0;
    build.hasTerminalAngularVelocity = true;
    build.terminalAngularVelocityMapRadPerSec =
        terminalAngularVelocityMapRadPerSec;
    build.policy.linearFeedbackReserveMps2 =
        linearReserve;
    build.policy.angularFeedbackReserveRadPerSec2 =
        angularReserve;

    const auto accepted =
        AcceptedManeuverProgramBuilder::build(build);
    if (!accepted.valid || accepted.pages.empty())
        return false;

    runtime.programs = accepted.pages;
    runtime.currentProgramPage = 0;
    runtime.nextProgramRevision +=
        static_cast<std::uint64_t>(runtime.programs.size());

    runtime.controlBridge =
        std::make_unique<NavigationRuntimeControlBridge>(
            NavigationRuntimeControlBridge::PilotSkillProfile {}
        );

    NavigationRuntimeControlBridge::Intent neutral;
    neutral.revision = runtime.requestSerial;
    neutral.targetRevision = runtime.programs.front().revision;
    if (!runtime.controlBridge->reset(
            universeTimeSeconds,
            neutral))
    {
        runtime.controlBridge.reset();
        runtime.programs.clear();
        return false;
    }

    runtime.phase =
        DockingAutomaticRuntime::Phase::Executing;

    std::cout
        << "[DockAuto] planned entity=" << runtime.entityId.value
        << " request=" << runtime.requestSerial
        << " pages=" << runtime.programs.size()
        << " trajectory_s="
        << trajectoryResult.trajectory.durationSeconds
        << " gates=" << advisoryPlan.gates.size()
        << " final_axis_m="
        << advisoryPlan.terminalApproachLengthMeters
        << " terminal_radius_m="
        << advisoryPlan.terminalTurnRadiusMeters
        << " pre_capture_depth_m="
        << finalPreCaptureDepthMeters
        << " terminal_t=" << captureUniverseTimeSeconds
        << " terminal_omega_radps="
        << glm::length(
            terminalAngularVelocityMapRadPerSec
        )
        << "\n";
    return true;
}


bool GameServer::finishAutomaticDocking(
    PlayerId playerId,
    EntityId controlledEntityId,
    std::uint64_t requestSerial,
    bool completed,
    const char* reason
)
{
    const auto it =
        m_dockingAutomaticRuntimes.find(
            controlledEntityId.value
        );
    if (it == m_dockingAutomaticRuntimes.end())
        return false;

    if (it->second.playerId != playerId ||
        it->second.requestSerial != requestSerial)
    {
        return false;
    }

    if (auto streamIt =
            m_controlStreams.find(controlledEntityId.value);
        streamIt != m_controlStreams.end())
    {
        streamIt->second.discardPendingAndAcknowledgeNewest();
    }

    if (Ship* ship = m_simulation.getShip(controlledEntityId))
        ship->setControlState(ShipControlState {});

    const bool restored =
        m_controls.restoreHumanControl(
            playerId,
            controlledEntityId
        );

    m_dockingAutomaticRuntimes.erase(it);
    m_forceSnapshotPublication = true;

    std::cout
        << "[DockAuto] "
        << (completed ? "approach-complete" : "cancelled")
        << " entity=" << controlledEntityId.value
        << " request=" << requestSerial
        << " reason=" << (reason ? reason : "none")
        << " human_restored=" << (restored ? 1 : 0)
        << "\n";
    return restored;
}


void GameServer::applyAutomaticDockingControls(
    const game::server::ServerTimeContext& time
)
{
    using Follower = game::navigation::TrajectoryFollower;
    using Timeline = game::navigation::ManeuverProgramTimeline;

    struct Completion
    {
        PlayerId playerId {};
        EntityId entityId {};
        std::uint64_t requestSerial = 0;
    };
    std::vector<Completion> completed;

    for (auto& [entityValue, runtime] :
         m_dockingAutomaticRuntimes)
    {
        Ship* ship =
            m_simulation.getShip(runtime.entityId);
        if (!ship ||
            m_controls.controllerKind(runtime.entityId) !=
                game::server::ControllerKind::Autopilot)
        {
            completed.push_back({
                runtime.playerId,
                runtime.entityId,
                runtime.requestSerial
            });
            continue;
        }

        if (auto streamIt =
                m_controlStreams.find(entityValue);
            streamIt != m_controlStreams.end())
        {
            streamIt->second.discardPendingAndAcknowledgeNewest();
        }

        const auto& transform = ship->core().transform();
        const auto& motion = transform.motion;
        const auto* hub =
            m_simulation.hubNavigationFrame(runtime.hubId);

        if (!hub ||
            !hub->valid ||
            hub->systemId != runtime.systemId ||
            motion.systemId != runtime.systemId ||
            !motion.matchedToReferenceFrame ||
            motion.matchedReferenceFrameId != runtime.hubId)
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.programs.clear();
            runtime.controlBridge.reset();
            runtime.settledSinceUniverseTimeSeconds = -1.0;

            ShipControlState stop;
            stop.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::BrakeToStop;
            ship->setControlState(stop);
            continue;
        }

        if (runtime.phase ==
            DockingAutomaticRuntime::Phase::Stabilizing)
        {
            ShipControlState stop;
            stop.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::BrakeToStop;
            ship->setControlState(stop);

            const double speed =
                glm::length(motion.localVelocityMps);
            const double angularRate = std::sqrt(
                static_cast<double>(transform.pitchRate) *
                    static_cast<double>(transform.pitchRate) +
                static_cast<double>(transform.yawRate) *
                    static_cast<double>(transform.yawRate) +
                static_cast<double>(transform.rollRate) *
                    static_cast<double>(transform.rollRate)
            );

            const double speedThreshold = std::max(
                0.05,
                static_cast<double>(
                    ship->core().descriptor().physics.
                        stopSpeedEpsilonMps
                )
            );

            if (speed > speedThreshold ||
                angularRate > 0.01)
            {
                runtime.settledSinceUniverseTimeSeconds = -1.0;
                continue;
            }

            if (runtime.settledSinceUniverseTimeSeconds < 0.0)
            {
                runtime.settledSinceUniverseTimeSeconds =
                    time.universeTimeSeconds;
                continue;
            }

            if (time.universeTimeSeconds -
                    runtime.settledSinceUniverseTimeSeconds <
                0.25)
            {
                continue;
            }

            if (time.universeTimeSeconds <
                runtime.nextPlanAttemptUniverseTimeSeconds)
            {
                continue;
            }

            if (!planAutomaticDocking(
                    runtime,
                    *ship,
                    time.universeTimeSeconds))
            {
                runtime.nextPlanAttemptUniverseTimeSeconds =
                    time.universeTimeSeconds + 0.50;
                std::cerr
                    << "[DockAuto] request="
                    << runtime.requestSerial
                    << " phase=plan-retry"
                    << " next_t="
                    << runtime.nextPlanAttemptUniverseTimeSeconds
                    << "\n";
            }
            continue;
        }

        if (runtime.programs.empty() ||
            !runtime.controlBridge)
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.settledSinceUniverseTimeSeconds = -1.0;
            continue;
        }

        const auto selection =
            Timeline::selectActivePage(
                runtime.programs.data(),
                runtime.programs.size(),
                time.universeTimeSeconds,
                runtime.currentProgramPage
            );

        if (selection.status !=
            Timeline::SelectionStatus::Active)
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.programs.clear();
            runtime.controlBridge.reset();
            runtime.settledSinceUniverseTimeSeconds = -1.0;
            continue;
        }

        runtime.currentProgramPage =
            selection.pageIndex;
        const auto& program =
            runtime.programs[runtime.currentProgramPage];

        if (time.universeTimeSeconds >
            program.validUntilUniverseTimeSeconds)
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.programs.clear();
            runtime.controlBridge.reset();
            runtime.settledSinceUniverseTimeSeconds = -1.0;
            continue;
        }

        Follower::AgentState agent;
        agent.positionMapMeters =
            motion.localPositionMeters;
        agent.velocityMapMetersPerSecond =
            motion.localVelocityMps;
        agent.forwardMap =
            hub->worldToLocalVector(
                glm::dvec3(transform.forward())
            );
        agent.rightMap =
            hub->worldToLocalVector(
                glm::dvec3(transform.right())
            );
        agent.upMap =
            hub->worldToLocalVector(
                glm::dvec3(transform.up())
            );
        agent.pitchRateRadPerSec =
            transform.pitchRate;
        agent.yawRateRadPerSec =
            transform.yawRate;
        agent.rollRateRadPerSec =
            transform.rollRate;

        const auto followed =
            Follower::follow(
                program,
                time.universeTimeSeconds,
                agent,
                runtime.trackingPolicy
            );

        if (followed.status ==
                Follower::Status::InvalidInput ||
            followed.trackingErrorExceeded ||
            !followed.propulsionFeasible)
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.programs.clear();
            runtime.controlBridge.reset();
            runtime.settledSinceUniverseTimeSeconds = -1.0;

            ShipControlState stop;
            stop.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::BrakeToStop;
            ship->setControlState(stop);

            std::cerr
                << "[DockAuto] request="
                << runtime.requestSerial
                << " phase=replan"
                << " tracking_error="
                << (followed.trackingErrorExceeded ? 1 : 0)
                << " propulsion_ok="
                << (followed.propulsionFeasible ? 1 : 0)
                << "\n";
            continue;
        }

        const game::navigation::NavigationFrameBoundary boundary(
            hub->kinematicFrame()
        );
        if (!boundary.valid())
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.programs.clear();
            runtime.controlBridge.reset();
            runtime.settledSinceUniverseTimeSeconds = -1.0;
            continue;
        }

        const auto systemIntent =
            boundary.toSystemControlIntent(
                followed.intent
            );

        const auto step =
            runtime.controlBridge->step(
                time.universeTimeSeconds,
                std::max(1.0e-6, time.gameplayDeltaSeconds),
                systemIntent
            );

        if (step.status !=
                game::navigation::
                    NavigationRuntimeControlBridge::
                        PilotExecutor::Status::Ok ||
            !step.snapshot.valid)
        {
            runtime.phase =
                DockingAutomaticRuntime::Phase::Stabilizing;
            runtime.programs.clear();
            runtime.controlBridge.reset();
            runtime.settledSinceUniverseTimeSeconds = -1.0;
            continue;
        }

        ship->setControlState(step.control);

        const bool finalPage =
            runtime.currentProgramPage + 1 ==
                runtime.programs.size();
        if (finalPage &&
            followed.status ==
                Follower::Status::Complete)
        {
            completed.push_back({
                runtime.playerId,
                runtime.entityId,
                runtime.requestSerial
            });
        }
    }

    for (const auto& completion : completed)
    {
        (void)finishAutomaticDocking(
            completion.playerId,
            completion.entityId,
            completion.requestSerial,
            true,
            "pre-capture-envelope-complete"
        );
    }
}


void GameServer::resetSessionControlState(
    EntityId controlledEntityId,
    const char* reason
)
{
    if (controlledEntityId.value == 0)
        return;

    m_dockingGuidancePreparations.erase(
        controlledEntityId.value
    );
    m_dockingAutomaticRuntimes.erase(
        controlledEntityId.value
    );

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

    if (const auto prepIt =
            m_dockingGuidancePreparations.find(controlledEntityId.value);
        prepIt != m_dockingGuidancePreparations.end())
    {
        const auto prep = prepIt->second;
        (void)finishDockingGuidancePreparation(
            prep.playerId, prep.entityId, prep.requestSerial, false);
    }

    if (const auto automaticIt =
            m_dockingAutomaticRuntimes.find(controlledEntityId.value);
        automaticIt != m_dockingAutomaticRuntimes.end())
    {
        const PlayerId automaticPlayer =
            automaticIt->second.playerId;
        const std::uint64_t automaticSerial =
            automaticIt->second.requestSerial;
        (void)finishAutomaticDocking(
            automaticPlayer,
            controlledEntityId,
            automaticSerial,
            false,
            "session-disconnect"
        );
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
        [this, controlledEntityId, playerId](const auto& payload)
        {
            using PayloadT = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<PayloadT, ShipControlState>)
            {
                submitCommand(controlledEntityId, payload);
            }
            else if constexpr (std::is_same_v<PayloadT, ClientShipCommand>)
            {
                if (payload.type == ClientShipCommand::BeginDockingGuidancePreparation)
                {
                    (void)beginDockingGuidancePreparation(
                        playerId, controlledEntityId, payload.requestSerial);
                    return;
                }
                if (payload.type == ClientShipCommand::CancelDockingGuidancePreparation)
                {
                    (void)finishDockingGuidancePreparation(
                        playerId, controlledEntityId, payload.requestSerial, false);
                    return;
                }
                if (payload.type == ClientShipCommand::CompleteDockingGuidancePreparation)
                {
                    (void)finishDockingGuidancePreparation(
                        playerId, controlledEntityId, payload.requestSerial, true);
                    return;
                }
                if (payload.type == ClientShipCommand::BeginAutomaticDocking)
                {
                    (void)beginAutomaticDocking(
                        playerId,
                        controlledEntityId,
                        payload
                    );
                    return;
                }
                if (payload.type == ClientShipCommand::CancelAutomaticDocking)
                {
                    (void)finishAutomaticDocking(
                        playerId,
                        controlledEntityId,
                        payload.requestSerial,
                        false,
                        "client-cancel"
                    );
                    return;
                }

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

bool GameServer::controlledEntityAutopilotActiveForSession(
    game::network::ServerSessionId sessionId
) const noexcept
{
    const EntityId controlledEntityId =
        controlledEntityForSession(sessionId);
    return controlledEntityId.value != 0 &&
        m_controls.controllerKind(controlledEntityId) ==
            game::server::ControllerKind::Autopilot;
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
    outSnapshot.session.controlledEntityAutopilotActive =
        controlledEntityAutopilotActiveForSession(sessionId);

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
    outSnapshot.session.controlledEntityAutopilotActive =
        controlledEntityAutopilotActiveForSession(sessionId);
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
    outSnapshot.session.controlledEntityAutopilotActive =
        controlledEntityAutopilotActiveForSession(sessionId);
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
