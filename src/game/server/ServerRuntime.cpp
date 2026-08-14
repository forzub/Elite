#include "src/game/server/ServerRuntime.h"

#include <stdexcept>

#include "src/game/debug/DebugSessionMessage.h"
#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"
#include "src/game/server/GameServer.h"

namespace game::server
{
ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    game::debug::IServerDebugChannel& debugChannel
)
    : m_server(std::make_unique<GameServer>())
    , m_debugChannel(debugChannel)
{
    // Bootstrap configuration belongs to the authoritative runtime. The host
    // must not mutate GameServer/world state directly, because that access
    // cannot survive a process boundary.
    m_server->world() = worldParams;
    m_server->update(0.0);

    // A dedicated runtime may legitimately have zero gameplay transports until
    // the first TCP connection is accepted. The same deterministic runner then
    // owns every admitted session and still advances one authoritative world.
    m_runner = std::make_unique<ServerRunner>(*m_server);

    m_debugChannel.publishSnapshot(m_server->snapshot());
    m_debugChannel.publishState(makeDebugState());
}

ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    IServerTransport& transport,
    game::debug::IServerDebugChannel& debugChannel
)
    : ServerRuntime(worldParams, debugChannel)
{
    // Embedded/local play preserves its established primary-session semantics.
    m_primarySessionId =
        m_server->createPlayerSession(m_server->playerId());

    if (!m_primarySessionId ||
        !m_runner->attachTransport(transport, m_primarySessionId))
    {
        throw std::runtime_error(
            "failed to create/bind primary server session"
        );
    }

    if (!publishSessionBootstrap(transport, m_primarySessionId))
    {
        throw std::runtime_error(
            "failed to publish primary server session bootstrap"
        );
    }
}

ServerRuntime::~ServerRuntime() = default;

ServerAdvanceResult ServerRuntime::advance(double elapsedSeconds)
{
    // Debug commands are authoritative requests. Process them at the runtime
    // boundary before the normal fixed-step loop, then expose only copied
    // diagnostic responses after the server has had a chance to publish the
    // requested state on its normal simulation cadence.
    receiveDebugCommands();

    const auto result = m_runner->advance(elapsedSeconds);

    publishPendingDebugSnapshot();

    if (m_debugStateDirty)
    {
        m_debugChannel.publishState(makeDebugState());
        m_debugStateDirty = false;
    }

    return result;
}

double ServerRuntime::fixedStepSeconds() const
{
    return m_runner->fixedStepSeconds();
}

bool ServerRuntime::publishSessionBootstrap(
    IServerTransport& transport,
    game::network::ServerSessionId sessionId
)
{
    const EntityId controlledEntityId =
        m_server->controlledEntityForSession(sessionId);

    if (!sessionId || controlledEntityId.value == 0)
        return false;

    // Session authority is bootstrap metadata, not recurring replicated state.
    // A packet never contains a caller-selected controlled EntityId.
    game::network::SessionWelcome welcome;
    welcome.sessionId = sessionId;
    welcome.controlledEntityId = controlledEntityId;
    welcome.fixedStepSeconds = m_runner->fixedStepSeconds();
    welcome.starAtlasCatalog.schemaVersion =
        world::celestial::StarAtlasDatabase::CatalogSchemaVersion;
    welcome.starAtlasCatalog.contentFingerprint =
        m_server->starAtlas().contentFingerprint();
    transport.publishSessionWelcomeImmediately(welcome);

    SimulationSnapshot bootstrapSnapshot;
    if (!m_server->copyHydratedSnapshotForSession(
            sessionId,
            bootstrapSnapshot))
    {
        return false;
    }

    // Seed the connection's sparse-publication memory from the exact full
    // baseline that is about to be delivered. A late join therefore starts
    // with complete retained graph/runtime state before any omission is legal.
    if (!m_runner->seedTransportReplicationBaseline(
            sessionId,
            bootstrapSnapshot))
    {
        return false;
    }

    // The first authoritative snapshot for each connection is bootstrap data,
    // not a latency-simulated gameplay packet. Its session navigation view is
    // already composed from that connection's controlled entity.
    transport.publishSnapshotImmediately(bootstrapSnapshot);
    return true;
}

game::network::ServerSessionId
ServerRuntime::attachPlayerSessionTransport(
    IServerTransport& transport
)
{
    const EntityId controlledEntityId =
        m_server->selectAvailablePlayerEntityForAdmission();

    if (controlledEntityId.value == 0)
        return {};

    return attachPlayerSessionTransport(transport, controlledEntityId);
}

game::network::ServerSessionId
ServerRuntime::attachPlayerSessionTransport(
    IServerTransport& transport,
    EntityId controlledEntityId
)
{
    const auto sessionId =
        m_server->createPlayerSession(controlledEntityId);

    if (!sessionId)
        return {};

    if (!m_runner->attachTransport(transport, sessionId))
    {
        m_server->disconnectPlayerSession(sessionId);
        return {};
    }

    if (!publishSessionBootstrap(transport, sessionId))
    {
        m_runner->detachTransport(sessionId);
        m_server->disconnectPlayerSession(sessionId);
        return {};
    }

    return sessionId;
}

bool ServerRuntime::detachPlayerSessionTransport(
    game::network::ServerSessionId sessionId
)
{
    if (!sessionId || sessionId == m_primarySessionId)
    {
        // The embedded/local primary connection currently shares the runtime
        // lifetime. Remote/non-primary sessions may disconnect independently;
        // primary hot-detach can be added with the later host/session lifecycle.
        return false;
    }

    if (!m_runner->detachTransport(sessionId))
        return false;

    return m_server->disconnectPlayerSession(sessionId);
}

std::size_t ServerRuntime::connectedPlayerSessionCount() const noexcept
{
    return m_server->connectedPlayerSessionCount();
}

void ServerRuntime::receiveDebugCommands()
{
    game::debug::DebugCommand command;
    bool requestSnapshot = false;
    bool requireFullSnapshot = false;

    while (m_debugChannel.receiveCommand(command))
    {
        using game::debug::DebugCommandType;

        switch (command.type)
        {
            case DebugCommandType::RefreshSnapshot:
                requestSnapshot = true;
                break;

            case DebugCommandType::RefreshStructureSnapshot:
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::DestroyShipModule:
                m_server->debugDestroyShipModule(command.shipId, command.itemId);
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::RestoreShipModule:
                m_server->debugRestoreShipModule(command.shipId, command.itemId);
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::ResetShipStructure:
                m_server->debugResetShipStructure(command.shipId);
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::ResetAllShipStructures:
                m_server->debugResetAllShipStructures();
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::DetachShipModule:
                m_server->debugDetachShipModule(command.shipId, command.itemId);
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::HangShipModule:
                m_server->debugHangShipModule(command.shipId, command.itemId);
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::ReevaluateShipStructure:
                m_server->debugReevaluateShipStructure(command.shipId);
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::SetShipStructuralLinkHealth:
                m_server->debugSetShipStructuralLinkHealth(
                    command.shipId,
                    command.itemId,
                    command.health,
                    command.destroyed
                );
                requestSnapshot = true;
                requireFullSnapshot = true;
                break;

            case DebugCommandType::SetUniverseTimeSimulation:
                m_server->setDebugUniverseTimeSimulation(
                    command.enabled,
                    command.timeScale
                );
                m_debugStateDirty = true;
                break;
        }
    }

    if (requestSnapshot)
    {
        m_debugSnapshotBaseServerTick =
            m_server->snapshot().metadata.serverTick;
        m_debugSnapshotPending = true;

        if (requireFullSnapshot)
        {
            // Structural debug data is intentionally absent from ordinary light
            // snapshots. Opt into one full publication only when a structure
            // tool actually asks for it; ordinary debug observation must not
            // perturb normal replication cadence.
            m_server->debugRefreshSnapshot();
        }
    }
}

void ServerRuntime::publishPendingDebugSnapshot()
{
    if (!m_debugSnapshotPending)
        return;

    const auto& snapshot = m_server->snapshot();

    // Ordinary debug observation waits for the next normal replication
    // publication; structural tools may have forced that publication above.
    // Either way, never satisfy a request with the same stale server tick.
    if (snapshot.metadata.serverTick == m_debugSnapshotBaseServerTick)
        return;

    m_debugChannel.publishSnapshot(snapshot);
    m_debugSnapshotPending = false;
}

game::debug::DebugSessionState ServerRuntime::makeDebugState() const
{
    game::debug::DebugSessionState state;
    state.fastUniverseTime = m_server->debugFastUniverseTime();
    state.universeTimeSimulation = m_server->debugUniverseTimeSimulation();
    state.universeTimeScale = m_server->debugUniverseTimeScale();
    state.configuredUniverseTimeScale =
        m_server->debugUniverseTimeConfiguredScale();
    return state;
}
}
