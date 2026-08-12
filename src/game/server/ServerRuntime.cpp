#include "src/game/server/ServerRuntime.h"

#include "src/game/debug/DebugSessionMessage.h"
#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"
#include "src/game/server/GameServer.h"

namespace game::server
{
ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    IServerTransport& transport,
    game::debug::IServerDebugChannel& debugChannel
)
    : m_server(std::make_unique<GameServer>())
    , m_debugChannel(debugChannel)
{
    // Bootstrap configuration belongs to the authoritative runtime. The host
    // must not mutate GameServer/world state directly, because that access
    // cannot survive a later server-thread boundary.
    m_server->world() = worldParams;

    // Preserve the established startup semantics: apply configured world
    // parameters before the first host-visible authoritative publication.
    m_server->update(0.0);

    m_runner = std::make_unique<ServerRunner>(
        *m_server,
        transport
    );

    // Session authority is bootstrap metadata, not recurring replicated state.
    // The client learns which entity it controls from the server endpoint and
    // never selects that EntityId in its command packets.
    game::network::SessionWelcome welcome;
    welcome.controlledEntityId = m_server->playerId();
    transport.publishSessionWelcomeImmediately(welcome);

    // The first authoritative snapshot is bootstrap data, not a
    // latency-simulated gameplay packet.
    transport.publishSnapshotImmediately(m_server->snapshot());

    // Debug tools get value copies through a separate diagnostic channel.
    // They must never hold references into GameServer just because local play
    // currently happens in one OS thread.
    m_debugChannel.publishSnapshot(m_server->snapshot());
    m_debugChannel.publishState(makeDebugState());
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
