#include "GameServer.h"
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

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/world_state/InitialWorldState.h"
#include "src/game/navigation/GalaxyNavigationConfig.h"
#include "src/game/navigation/PlayerSpatialDomainResolver.h"



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


namespace
{
    world::celestial::LocalSceneAxes axesToHubLocal(
        const glm::mat4& worldOrientation,
        const game::navigation::HubNavigationFrame& frame
    )
    {
        world::celestial::LocalSceneAxes axes;

        auto convert =
            [&](const glm::dvec3& worldAxis)
            {
                return glm::dvec3(
                    glm::dot(worldAxis, frame.progradeAxis),
                    glm::dot(worldAxis, frame.radialAxis),
                    glm::dot(worldAxis, frame.normalAxis)
                );
            };

        axes.x = convert(glm::dvec3(worldOrientation[0]));
        axes.y = convert(glm::dvec3(worldOrientation[1]));
        axes.z = convert(glm::dvec3(worldOrientation[2]));

        return axes;
    }

    glm::dvec3 velocityToHubLocal(
        const glm::dvec3& worldVelocity,
        const game::navigation::HubNavigationFrame& frame
    )
    {
        const glm::dvec3 relative =
            worldVelocity - frame.velocityMetersPerSecond;

        return glm::dvec3(
            glm::dot(relative, frame.progradeAxis),
            glm::dot(relative, frame.radialAxis),
            glm::dot(relative, frame.normalAxis)
        );
    }
}














GameServer::GameServer()
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

        m_playerNavigation.currentSystemId =
            initialSystemId;

        m_celestialRuntimes.initialize(m_starAtlas);




const double universeTime =
    m_universeClock.timeSeconds();


std::unordered_map<std::string, glm::dvec3>
    currentCelestialPositionsAu;
std::unordered_map<std::string, glm::dvec3>
    currentCelestialVelocitiesAuPerSecond;

if (const auto* celestial =
        celestialSnapshotForSystem(
            m_playerNavigation.currentSystemId
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
    m_playerNavigation.currentSystemId,
    currentCelestialPositionsAu,
    currentCelestialVelocitiesAuPerSecond
);











        m_simulation.buildInitialScene(initialWorldState);
        synchronizePlayerSystemMembership();

        applyCelestialOrbitParentParameters();

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


}







void GameServer::synchronizePlayerSystemMembership()
{
    const Ship* player = m_simulation.playerShip();
    if (!player)
        return;

    const int shipSystemId =
        player->core().transform().motion.systemId;

    if (shipSystemId >= 0)
        m_playerNavigation.currentSystemId = shipSystemId;
}


const world::celestial::CelestialSystemSnapshot*
GameServer::celestialSnapshotForSystem(int systemId) const
{
    return m_celestialRuntimes.resolve(
        systemId,
        m_universeClock.timeSeconds()
    );
}


void GameServer::applyCelestialOrbitParentParameters()
{
    const int systemId =
        m_playerNavigation.currentSystemId;

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

// The player's current system is entity authority, not an independent server
// navigation variable. Resolve the active celestial context from membership
// before injecting this tick's kinematics.
synchronizePlayerSystemMembership();

std::unordered_map<std::string, glm::dvec3> celestialPositionsAu;
std::unordered_map<std::string, glm::dvec3>
    celestialVelocitiesAuPerSecond;

if (const auto* celestial =
        celestialSnapshotForSystem(
            m_playerNavigation.currentSystemId
        ))
{
    for (const auto& state : celestial->bodies)
    {
        celestialPositionsAu[state.id] = state.positionAu;
        celestialVelocitiesAuPerSecond[state.id] =
            state.velocityAuPerSecond;
    }
}

m_simulation.setCelestialBodyKinematicStateAu(
    m_playerNavigation.currentSystemId,
    celestialPositionsAu,
    celestialVelocitiesAuPerSecond
);

if (m_appliedSimulationContextSystemId !=
    m_playerNavigation.currentSystemId)
{
    applyCelestialOrbitParentParameters();
}

m_simulation.update(time);



    if (m_simulation.playerShip())
    {
        const ShipTransform tr =
            m_simulation.presentationShipTransform(
                m_simulation.playerId()
            );

        const auto spatialDomain =
            game::navigation::resolvePlayerSpatialDomain(
                m_starAtlas.systems(),
                tr.motion.systemId,
                tr.worldPosition,
                m_systemMembershipRadiusAu
            );

        if (spatialDomain.valid)
        {
            m_playerNavigation.currentSystemId =
                spatialDomain.currentSystemId;
            m_playerNavigation.worldPosition =
                spatialDomain.worldPosition;
            m_playerNavigation.systemLocalMeters =
                spatialDomain.systemLocalMeters;
            m_playerNavigation.systemLocalAu =
                spatialDomain.systemLocalAu;
        }
        else
        {
            // Catalog/source mismatch is not allowed to fabricate a spatial
            // transfer. Preserve the production entity membership and local
            // coordinates as a safe fallback.
            m_playerNavigation.currentSystemId = tr.motion.systemId;
            m_playerNavigation.worldPosition = tr.worldPosition;
            m_playerNavigation.systemLocalMeters =
                world::coordinates::fullMeters(tr.worldPosition);
            m_playerNavigation.systemLocalAu =
                m_playerNavigation.systemLocalMeters /
                world::celestial::MetersPerAu;
        }

        m_playerNavigation.orientation = tr.orientation;
        m_playerNavigation.forward = tr.forward();
        m_playerNavigation.up = tr.up();
    }











    if (m_forceSnapshotPublication ||
        m_serverTick % m_snapshotInterval == 0)
    {
        // Snapshot construction belongs to the publication cadence. The
        // simulation step above mutates authoritative state only; replication
        // DTOs are materialized here when they can actually be delivered.
        m_lastSnapshot =
            m_simulation.buildReplicationSnapshot(m_serverTick);
        populateClientSessionSnapshot(m_lastSnapshot);
        m_forceSnapshotPublication = false;
    }

    processPendingMapRequests();
}

void GameServer::enqueueMapRequest(const game::network::MapRequest& request)
{
    if (m_pendingMapRequests.size() >= MaxPendingMapRequests)
    {
        m_pendingMapRequests.pop_front();
        ++m_queueDiagnostics.droppedMapRequests;
    }

    m_pendingMapRequests.push_back(request);
}

bool GameServer::popMapResponse(game::network::MapResponse& outResponse)
{
    if (m_completedMapResponses.empty())
        return false;
    outResponse = std::move(m_completedMapResponses.front());
    m_completedMapResponses.pop_front();
    return true;
}

void GameServer::processPendingMapRequests()
{
    const auto metadata = protocolMetadata();
    while (!m_pendingMapRequests.empty())
    {
        auto request = std::move(m_pendingMapRequests.front());
        m_pendingMapRequests.pop_front();
        std::visit([this, &metadata](const auto& typedRequest)
        {
            using RequestT = std::decay_t<decltype(typedRequest)>;
            if constexpr (std::is_same_v<RequestT, game::network::GalaxyMapRequest>)
            {
                game::network::GalaxyMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata;
                response.snapshot = buildGalaxyMapSnapshot();
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
            else if constexpr (std::is_same_v<RequestT, game::network::SystemMapRequest>)
            {
                game::network::SystemMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata; response.systemId = typedRequest.systemId;
                response.snapshot = buildSystemMapSnapshot(typedRequest.systemId);
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
            else if constexpr (std::is_same_v<RequestT, game::network::DetailMapRequest>)
            {
                game::network::DetailMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata; response.target = typedRequest.target;
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
            else if constexpr (std::is_same_v<RequestT, game::network::HubMapRequest>)
            {
                game::network::HubMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata; response.systemId = typedRequest.systemId; response.hubId = typedRequest.hubId;
                response.snapshot = buildHubMapSnapshot(typedRequest.systemId, typedRequest.hubId);
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
        }, request);
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

    snapshot.session.playerNavigation = m_playerNavigation;
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





void GameServer::receiveClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    std::visit(
        [this, playerId](const auto& payload)
        {
            using PayloadT = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<PayloadT, ShipControlState>)
            {
                submitCommand(playerId, payload);
            }
            else if constexpr (std::is_same_v<PayloadT, ClientShipCommand>)
            {
                auto& queue = m_pendingClientShipCommands[playerId.value];
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





    glm::dvec3 assemblySizeMetersForType(
        ObjectType typeId
    )
    {
        using game::ship::geometry::AssemblyMeshLibrary;

        if (typeId == ObjectType::None)
            return glm::dvec3(1.0);

        if (!AssemblyMeshLibrary::has(typeId))
            return glm::dvec3(1.0);

        const auto& assembly =
            AssemblyMeshLibrary::get(typeId);

        const glm::vec3 size =
            assembly.maxBounds - assembly.minBounds;

        return glm::dvec3(
            std::max(1.0f, size.x),
            std::max(1.0f, size.y),
            std::max(1.0f, size.z)
        );
    }
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



world::celestial::HubMapSnapshot
GameServer::buildHubMapSnapshot(
    int systemId,
    const std::string& hubId
) const
{
    using namespace world::celestial;

    HubMapSnapshot out;

    out.systemId =
        systemId;

    out.hubId =
        hubId;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    auto hubIt =
        m_simulation
            .orbitalHubs()
            .find(
                hubId
            );

    if (hubIt ==
        m_simulation
            .orbitalHubs()
            .end())
    {
        return out;
    }

    const auto& hub =
        hubIt->second;

    if (hub.systemId != systemId)
        return out;

    const auto* frame =
        m_simulation.hubNavigationFrame(
            hubId
        );

    if (!frame ||
        !frame->valid ||
        frame->systemId != systemId)
    {
        return out;
    }

    out.parentBodyId =
        hub.parentBodyId;

    if (const auto* summary =
            m_starAtlas.findSystemSummary(
                systemId
            ))
    {
        out.systemPositionLy =
            summary->positionLy;
    }

    const auto* system =
        m_starAtlas.findSystem(
            systemId
        );

    if (system)
    {
        for (const auto& body :
             system->bodies)
        {
            if (body.id !=
                out.parentBodyId)
            {
                continue;
            }

            out.parentEnvironmentPresetId =
                body.environmentPresetId;

            out.parentPlanetAxialTiltDeg =
                body.axialTiltDeg;

            out.parentPlanetAxisNodeDeg =
                body.axisNodeDeg;

            out.parentPlanetTextureLongitudeOffsetDeg =
                body.textureLongitudeOffsetDeg;

            break;
        }
    }

    out.displayName =
        hub.name.empty()
            ? hub.id
            : hub.name;

    out.scene.anchorClass = DetailObjectClass::Hub;
    out.scene.anchorId = hubId;
    out.scene.focusId = hubId;
    out.scene.coordinateSpace =
        LocalSceneCoordinateSpace::AnchorLocalMeters;
    out.scene.originWorldMeters = frame->originMeters;

    /*
        Локальная Hub Map convention:

            X = prograde;
            Y = radial;
            Z = normal.
    */
    out.hubAxes.x =
        glm::dvec3(
            1.0,
            0.0,
            0.0
        );

    out.hubAxes.y =
        glm::dvec3(
            0.0,
            1.0,
            0.0
        );

    out.hubAxes.z =
        glm::dvec3(
            0.0,
            0.0,
            1.0
        );

    /*
        Эти значения являются параметрами определения орбиты.

        Текущий фактический radius и altitude дополнительно
        пересчитываются в refreshHubMapDynamicState().
    */
    out.parentPlanetRadiusMeters =
        hub.motion.parentRadiusMeters;

    out.hubAltitudeMeters =
        hub.motion.altitudeMeters;

    out.hubOrbitRadiusMeters =
        hub.motion.parentRadiusMeters +
        hub.motion.altitudeMeters;

    /*
        Теперь snapshot считается валидным по статической части.

        Все текущие координаты, оси, modules и ships
        строятся в единственной функции ниже.
    */
    out.valid =
        true;

    refreshHubMapDynamicState(
        out
    );

    return out;
}



void GameServer::refreshHubMapDynamicState(
    world::celestial::HubMapSnapshot& snapshot
) const
{
    using namespace world::celestial;

    if (snapshot.systemId < 0 ||
        snapshot.hubId.empty())
    {
        return;
    }

    auto hubIt =
        m_simulation
            .orbitalHubs()
            .find(
                snapshot.hubId
            );

    if (hubIt ==
        m_simulation
            .orbitalHubs()
            .end())
    {
        snapshot.valid =
            false;

        return;
    }

    const auto& hub =
        hubIt->second;

    const auto* frame =
        m_simulation.hubNavigationFrame(
            snapshot.hubId
        );

    if (!frame ||
        !frame->valid)
    {
        snapshot.valid =
            false;

        return;
    }

    /*
        Один временной срез для всего snapshot.
    */
    const double currentUniverseTimeSeconds =
        m_universeClock.timeSeconds();

    snapshot.universeTimeSeconds =
        currentUniverseTimeSeconds;

    // ------------------------------------------------------------
    // 1. Текущее абсолютное состояние хаба.
    // ------------------------------------------------------------

    snapshot.hubWorldPositionMeters =
        frame->originMeters;

    snapshot.scene.originWorldMeters =
        frame->originMeters;

    snapshot.hubWorldVelocityMps =
        frame->velocityMetersPerSecond;

    snapshot.hubWorldAxes.x =
        frame->progradeAxis;

    snapshot.hubWorldAxes.y =
        frame->radialAxis;

    snapshot.hubWorldAxes.z =
        frame->normalAxis;

    // ------------------------------------------------------------
    // 2. Родительская планета из того же CelestialRuntimeRegistry.
    // ------------------------------------------------------------

    const auto* celestial =
        celestialSnapshotForSystem(snapshot.systemId);

    if (!celestial)
    {
        snapshot.valid = false;
        return;
    }

    const world::celestial::CelestialBodyState* parentPlanet = nullptr;

    for (const auto& body : celestial->bodies)
    {
        if (body.id == snapshot.parentBodyId)
        {
            parentPlanet = &body;
            break;
        }
    }

    if (!parentPlanet)
    {
        snapshot.valid = false;
        return;
    }

    snapshot.parentPlanetWorldPositionMeters =
        parentPlanet->worldMeters;
    snapshot.parentPlanetWorldVelocityMps =
        parentPlanet->worldVelocityMetersPerSecond;
    snapshot.parentPlanetCenterLocalMeters =
        frame->worldToLocalPosition(parentPlanet->worldMeters);

    if (parentPlanet->radiusKm > 0.0)
    {
        snapshot.parentPlanetRadiusMeters =
            parentPlanet->radiusKm * 1000.0;
    }

    snapshot.parentPlanetRotationPhaseRad =
        parentPlanet->rotationPhaseRad;
    snapshot.parentPlanetAxialTiltDeg =
        parentPlanet->axialTiltDeg;
    snapshot.parentPlanetAxisNodeDeg =
        parentPlanet->axisNodeDeg;
    snapshot.parentPlanetTextureLongitudeOffsetDeg =
        parentPlanet->textureLongitudeOffsetDeg;

    // Authored radius is only a guide parameter. It never replaces the
    // authoritative relative pose above.
    snapshot.hubOrbitRadiusMeters =
        std::max(
            1.0,
            snapshot.parentPlanetRadiusMeters +
                hub.motion.altitudeMeters
        );
    snapshot.hubAltitudeMeters = hub.motion.altitudeMeters;

    const double measuredOrbitRadiusMeters =
        glm::length(snapshot.parentPlanetCenterLocalMeters);
    const double authoredOrbitRadiusMeters =
        snapshot.hubOrbitRadiusMeters;

    // ------------------------------------------------------------
    // 4. Modules.
    //
    // Полностью пересобираем массив из текущего server state.
    // Так из snapshot исчезают удалённые или перемещённые модули.
    // ------------------------------------------------------------

    snapshot.scene.objects.clear();

    for (const auto& [id, object] :
         m_simulation.staticObjects())
    {
        if (object.systemId != snapshot.systemId ||
            object.hubId !=
            snapshot.hubId)
        {
            continue;
        }

        HubMapModule module;

        module.id =
            id;

        module.typeId =
            object.type;

        module.stableId =
            std::to_string(
                id.value
            );

        module.name =
            object.displayName.empty()
                ? "Hub module"
                : object.displayName;

        module.kind =
            object.type ==
                ObjectType::Station
                    ? "station"
                    : "module";
        module.objectClass = DetailObjectClass::Hub;
        module.role = LocalSceneObjectRole::Component;
        module.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        module.parentStableId = snapshot.hubId;

        const glm::dvec3 moduleWorldMeters =
            world::coordinates::fullMeters(
                object.worldPosition
            );

        module.positionMeters =
            frame->worldToLocalPosition(
                moduleWorldMeters
            );

        module.axes =
            axesToHubLocal(
                object.orientation,
                *frame
            );

        module.sizeMeters =
            assemblySizeMetersForType(
                object.type
            );

        module.prime =
            !hub.modules.empty() &&
            hub.modules.front().value ==
                id.value;

        module.valid =
            true;

        snapshot.scene.objects.push_back(
            module
        );
    }

    // ------------------------------------------------------------
    // 5. Ships.
    //
    // Hub map consumes the same presentation branch as the main simulation
    // snapshot. During accelerated diagnostics this includes every real ship
    // associated with the hub, while production transforms remain untouched.
    // ------------------------------------------------------------

    for (const auto& [entityId, shipPtr] :
         m_simulation.ships())
    {
        if (!shipPtr)
            continue;

        const bool isPlayer =
            entityId == m_simulation.playerId();

        const ShipTransform transform =
            m_simulation.presentationShipTransform(entityId);

        if (transform.motion.systemId != snapshot.systemId)
            continue;

        const bool usesThisHubFrame =
            transform.motion.hubId == snapshot.hubId;

        // Preserve the historical player marker even when inspecting the
        // current hub during a transition; other ships belong only to their
        // actual hub frame.
        if (!usesThisHubFrame && !isPlayer)
            continue;

        const glm::dvec3 shipWorldMeters =
            world::coordinates::fullMeters(
                transform.worldPosition
            );

        HubMapShip ship;
        ship.id = entityId;
        ship.typeId = shipPtr->typeId();
        const auto motionLabKind =
            m_simulation.hubMotionLabActorKind(entityId);

        ship.name =
            isPlayer
                ? "Player"
                : (motionLabKind !=
                        game::diagnostics::HubMotionLabActorKind::None
                    ? game::diagnostics::hubMotionLabLabel(motionLabKind)
                    : (shipPtr->core().descriptor().identity.shipName.empty()
                        ? "Ship " + std::to_string(entityId.value)
                        : shipPtr->core().descriptor().identity.shipName));
        ship.kind = "ship";
        ship.objectClass = DetailObjectClass::Ship;
        ship.role = LocalSceneObjectRole::Participant;
        ship.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        ship.parentStableId = snapshot.hubId;

        /*
            HubTactical is canonical in local coordinates. Diagnostic passive
            trajectories and all other modes are presentation world states and
            are projected into the requested hub frame here.
        */
        if (usesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            ship.positionMeters =
                transform.motion.localPositionMeters;
        }
        else
        {
            ship.positionMeters =
                frame->worldToLocalPosition(
                    shipWorldMeters
                );
        }

        if (usesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::Docked)
        {
            ship.velocityMps = glm::dvec3(0.0);
        }
        else if (
            usesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            ship.velocityMps =
                transform.motion.localVelocityMps;
        }
        else
        {
            ship.velocityMps =
                frame->worldToLocalVelocity(
                    shipWorldMeters,
                    transform.motion.worldVelocityMps
                );
        }

        ship.axes =
            axesToHubLocal(
                transform.orientation,
                *frame
            );

        ship.sizeMeters =
            assemblySizeMetersForType(
                ship.typeId
            );

        ship.player = isPlayer;
        ship.valid = true;

        snapshot.scene.objects.push_back(ship);

        if (isPlayer)
        {
            const glm::dvec3 reconstructedPlayerWorld =
                frame->localToWorldPosition(
                    ship.positionMeters
                );

            const double playerRoundTripErrorMeters =
                glm::length(
                    reconstructedPlayerWorld -
                    shipWorldMeters
                );

            if (playerRoundTripErrorMeters > 0.01)
            {
                auto& warningCount =
                    m_diagnostics.server.hubPlayerRoundTripWarnings;

                if (warningCount < 20)
                {
                    ++warningCount;

                    std::cerr
                        << "[HubMapConsistency]"
                        << " player round-trip error="
                        << playerRoundTripErrorMeters
                        << " m"
                        << " hub="
                        << snapshot.hubId
                        << "\n";
                }
            }
        }
    }


    if (game::diagnostics::HubMotionLabEnabled &&
        snapshot.systemId == game::diagnostics::HubMotionLabSystemId &&
        snapshot.hubId == game::diagnostics::HubMotionLabHubId)
    {
        const auto pose =
            game::diagnostics::evaluateHubMotionLabCube(
                m_simulation.serverTime()
            );

        HubMapShip cube;
        cube.stableId = "diagnostic:hub_motion_lab_cube";
        cube.name = "LAB ANALYTIC CUBE";
        cube.kind = "diagnostic_probe";
        cube.objectClass = DetailObjectClass::Ship;
        cube.role = LocalSceneObjectRole::Participant;
        cube.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        cube.parentStableId = snapshot.hubId;
        cube.positionMeters = pose.localPositionMeters;
        cube.sizeMeters = glm::dvec3(pose.halfExtentMeters * 2.0);
        cube.valid = true;
        snapshot.scene.objects.push_back(std::move(cube));
    }

    snapshot.scene.halfExtentMeters = 1000.0;

    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid)
            continue;

        const double objectExtent =
            std::max(
                object.boundingRadiusMeters,
                glm::length(object.sizeMeters) * 0.5
            );

        snapshot.scene.halfExtentMeters =
            std::max(
                snapshot.scene.halfExtentMeters,
                glm::length(object.positionMeters) +
                    objectExtent
            );
    }

    // ------------------------------------------------------------
    // 6. Контроль геометрии planet <-> hub.
    // ------------------------------------------------------------

    const double orbitRadiusErrorMeters =
        std::abs(
            measuredOrbitRadiusMeters -
            authoredOrbitRadiusMeters
        );

    const glm::dvec3 hubLocalOrigin =
        frame->worldToLocalPosition(
            snapshot.hubWorldPositionMeters
        );

    const double hubOriginErrorMeters =
        glm::length(
            hubLocalOrigin
        );

    if (orbitRadiusErrorMeters >
            1.0 ||
        hubOriginErrorMeters >
            0.01)
    {
        auto& warningCount =
            m_diagnostics.server.hubGeometryWarnings;

        if (warningCount <
            20)
        {
            ++warningCount;

            std::cerr
                << "[HubMapConsistency]"
                << " orbitRadiusError="
                << orbitRadiusErrorMeters
                << " m"
                << " hubOriginError="
                << hubOriginErrorMeters
                << " m"
                << " time="
                << snapshot.universeTimeSeconds
                << "\n";
        }
    }

    snapshot.valid =
        true;
}
