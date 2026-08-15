#include "GameServer.h"
#include "src/game/network/ReplicationSnapshotMerge.h"
#include <type_traits>
#include "src/game/network/ClientMessage.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#include "src/world/coordinates/WorldPosition.h"
#include <cmath>
#include <functional>
#include <unordered_map>
#include <utility>

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/world_state/InitialWorldState.h"
#include "src/game/navigation/GalaxyNavigationConfig.h"
#include "src/game/navigation/PlayerSpatialDomainResolver.h"
#include "src/game/ship/ShipInitData.h"



namespace {




    void appendSystemMapMotionDebugCsv(
        const world::celestial::CelestialSystemDefinition& system,
        const world::celestial::CelestialSystemSnapshot& celestial,
        double universeTimeSeconds,
        game::diagnostics::ServerDiagnostics& diagnostics
    )
    {
        if (!diagnostics.settings.systemMapMotionCsv)
            return;

        double& lastLoggedUniverseTime =
            diagnostics.server.systemMapLastLoggedUniverseTime;

        // This diagnostic is explicitly opt-in. Normal System-map requests no
        // longer resolve/build deterministic celestial presentation on the
        // authoritative server just so the client can draw it.
        if (lastLoggedUniverseTime >= 0.0 &&
            std::abs(universeTimeSeconds - lastLoggedUniverseTime) < 1.0)
        {
            return;
        }

        lastLoggedUniverseTime = universeTimeSeconds;

        const char* path = "system_map_motion.csv";
        std::ifstream check(path);
        const bool needHeader = !check.good();
        check.close();

        std::ofstream out(path, std::ios::app);
        if (!out.is_open())
            return;

        if (needHeader)
        {
            out
                << "universe_time,system_id,body_id,body_name,type,"
                << "x_au,y_au,z_au,orbit_radius_au,draw_orbit\n";
        }

        for (const auto& body : system.bodies)
        {
            if (body.type != world::celestial::BodyType::Planet &&
                body.type != world::celestial::BodyType::Moon)
            {
                continue;
            }

            glm::dvec3 positionAu = body.staticPositionAu;
            for (const auto& state : celestial.bodies)
            {
                if (state.id == body.id)
                {
                    positionAu = state.positionAu;
                    break;
                }
            }

            out
                << std::fixed
                << std::setprecision(6)
                << universeTimeSeconds
                << ","
                << system.systemId
                << ",\""
                << body.id
                << "\",\""
                << body.name
                << "\","
                << world::celestial::toString(body.type)
                << ","
                << std::setprecision(12)
                << positionAu.x
                << ","
                << positionAu.y
                << ","
                << positionAu.z
                << ","
                << body.distanceAu
                << ","
                << (body.distanceAu > 0.0 ? 1 : 0)
                << "\n";
        }
    }





































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

                if constexpr (std::is_same_v<RequestT, game::network::GalaxyMapRequest>)
                {
                    game::network::GalaxyMapResponse response;
                    response.requestId = typedRequest.requestId;
                    response.metadata = metadata;
                    response.snapshot = buildGalaxyMapSnapshot();
                    queueMapResponse(sessionId, std::move(response));
                }
                else if constexpr (std::is_same_v<RequestT, game::network::SystemMapRequest>)
                {
                    game::network::SystemMapResponse response;
                    response.requestId = typedRequest.requestId;
                    response.metadata = metadata;
                    response.systemId = typedRequest.systemId;
                    response.snapshot =
                        buildSystemMapSnapshot(typedRequest.systemId);
                    queueMapResponse(sessionId, std::move(response));
                }
                else if constexpr (std::is_same_v<RequestT, game::network::DetailMapRequest>)
                {
                    game::network::DetailMapResponse response;
                    response.requestId = typedRequest.requestId;
                    response.metadata = metadata;
                    response.target = typedRequest.target;
                    queueMapResponse(sessionId, std::move(response));
                }
                else if constexpr (std::is_same_v<RequestT, game::network::HubMapRequest>)
                {
                    game::network::HubMapResponse response;
                    response.requestId = typedRequest.requestId;
                    response.metadata = metadata;
                    response.systemId = typedRequest.systemId;
                    response.hubId = typedRequest.hubId;
                    queueMapResponse(sessionId, std::move(response));
                }
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

    m_simulation.setPlayerControlled(controlledEntityId, true);
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

    // Persistent player->ship control identity survives a disconnect, but the
    // expensive Active/player-controlled simulation pin is connection-scoped.
    // A replacement session for the same PlayerId will pin it again.
    if (!m_sessions.isConnectedPlayer(playerId))
        m_simulation.setPlayerControlled(controlledEntityId, false);

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
    const EntityId controlledEntityId =
        controlledEntityForSession(sessionId);

    if (controlledEntityId.value == 0)
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
                submitCommand(controlledEntityId, payload);
            }
            else if constexpr (std::is_same_v<PayloadT, ClientShipCommand>)
            {
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











world::celestial::SystemMapSnapshot
GameServer::buildSystemMapSnapshot(
    int systemId
) const
{
    world::celestial::SystemMapSnapshot out;

    const auto* system =
        m_starAtlas.findSystem(systemId);

    if (!system)
        return out;

    // The physical system id and authoritative epoch are enough for the
    // client to compose static System-map identity/placement from its local
    // StarAtlas.
    out.systemId = system->systemId;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    out.universeTimeScale =
        m_universeClock.timeScale();

    out.universeDate =
        m_universeClock.dateTimeString();








    /*
        System-map celestial bodies are deterministic catalog presentation.
        ClientMapService reconstructs them from its local StarAtlas and
        CelestialRuntimeRegistry at this response's universe-time epoch. This
        keeps predictable map geometry off the authoritative server. Production
        dynamic ships/hubs/infrastructure are joined from ordinary replication
        on the client at this response epoch.

        The motion CSV is an explicit server diagnostic, so it may resolve the
        celestial state on demand only when that diagnostic is enabled.
    */
    if (m_diagnostics.settings.systemMapMotionCsv)
    {
        if (const auto* celestial = celestialSnapshotForSystem(systemId))
        {
            appendSystemMapMotionDebugCsv(
                *system,
                *celestial,
                out.universeTimeSeconds,
                m_diagnostics
            );
        }
    }

    /*
        Production infrastructure/hubs are intentionally absent here. Their
        authoritative identity, ownership, bindings and transforms leave the
        server through ordinary SimulationSnapshot replication. ClientMapService
        samples that retained history at this response's exact server-time epoch
        and composes the System-map infrastructure layer locally.
    */

    /*
        Ordinary player/NPC ships are intentionally absent here. Their
        authoritative transforms already leave the server through the normal
        SimulationSnapshot stream. ClientMapService samples that retained
        replication history at this response's exact server-time epoch and
        constructs the System-map ship layer locally, avoiding a second
        network channel for the same moving entities.
    */


    // Presentation-only analytic reference used by Hub Motion Lab. It is not a
    // gameplay entity and has no physics state; maps evaluate the same shared
    // time function only so the reference remains visible while diagnosing
    // spatial-domain transitions.
    if (game::diagnostics::HubMotionLabEnabled &&
        systemId == game::diagnostics::HubMotionLabSystemId)
    {
        const auto* frame =
            m_simulation.hubNavigationFrame(
                std::string(game::diagnostics::HubMotionLabHubId)
            );

        if (frame && frame->valid)
        {
            const auto pose =
                game::diagnostics::evaluateHubMotionLabCube(
                    m_simulation.serverTime()
                );

            world::celestial::SystemMapObject cube;
            cube.stableId = "diagnostic:hub_motion_lab_cube";
            cube.name = "LAB ANALYTIC CUBE";
            cube.parentBodyId = frame->parentBodyId;
            cube.kind = world::celestial::SystemMapObjectKind::Ship;
            cube.positionAu =
                frame->localToWorldPosition(pose.localPositionMeters) /
                world::celestial::MetersPerAu;
            cube.systemId = frame->systemId;
            cube.hasOrbit = false;
            out.objects.push_back(std::move(cube));
        }
    }



    return out;
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
