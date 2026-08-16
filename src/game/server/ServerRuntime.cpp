#include "src/game/server/ServerRuntime.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "src/game/debug/DebugSessionMessage.h"
#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"
#include "src/game/identity/Sha256.h"
#include "src/game/server/GameServer.h"

namespace game::server
{
ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    game::debug::IServerDebugChannel& debugChannel
)
    : ServerRuntime(worldParams, debugChannel, 2)
{
}

ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    game::debug::IServerDebugChannel& debugChannel,
    std::size_t bootstrapPlayerSlotCount
)
    : m_server(std::make_unique<GameServer>(bootstrapPlayerSlotCount))
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
    : ServerRuntime(worldParams, debugChannel, 1)
{
    // Local play uses the same account identity seam as a remote client. The
    // loopback transport already contains SessionHello before ServerWorker
    // constructs the authoritative runtime. No local-only PlayerId shortcut.
    game::network::SessionHello hello;
    if (!transport.receiveSessionHello(hello))
    {
        throw std::runtime_error(
            "local server runtime started without client identity hello"
        );
    }

    m_primarySessionId = attachPlayerSessionTransport(transport, hello);
    if (!m_primarySessionId)
    {
        throw std::runtime_error(
            "failed to authenticate/bind primary local game session"
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
    const PlayerId playerId =
        m_server->playerForSession(sessionId);
    const ShipInstanceId controlledShipInstanceId =
        m_server->controlledShipInstanceForSession(sessionId);
    const EntityId controlledEntityId =
        m_server->controlledEntityForSession(sessionId);

    if (!sessionId || !playerId ||
        controlledShipInstanceId == 0 ||
        controlledEntityId.value == 0)
    {
        return false;
    }

    // Session authority is bootstrap metadata, not recurring replicated state.
    // A packet never contains a caller-selected controlled EntityId.
    game::network::SessionWelcome welcome;
    welcome.sessionId = sessionId;
    welcome.playerId = playerId;
    welcome.controlledShipInstanceId = controlledShipInstanceId;
    welcome.controlledEntityId = controlledEntityId;
    welcome.fixedStepSeconds = m_runner->fixedStepSeconds();
    welcome.starAtlasCatalog.schemaVersion =
        world::celestial::StarAtlasDatabase::CatalogSchemaVersion;
    welcome.starAtlasCatalog.contentFingerprint =
        m_server->starAtlas().contentFingerprint();
    using Clock = std::chrono::steady_clock;

    const auto welcomeBegin = Clock::now();
    transport.publishSessionWelcomeImmediately(welcome);
    const double welcomeMs = std::chrono::duration<double, std::milli>(
        Clock::now() - welcomeBegin
    ).count();

    SimulationSnapshot bootstrapSnapshot;
    const auto copyBegin = Clock::now();
    if (!m_server->copyHydratedSnapshotForSession(
            sessionId,
            bootstrapSnapshot))
    {
        return false;
    }

    const double copyMs = std::chrono::duration<double, std::milli>(
        Clock::now() - copyBegin
    ).count();

    // Seed the connection's sparse-publication memory from the exact full
    // baseline that is about to be delivered. A late join therefore starts
    // with complete retained graph/runtime state before any omission is legal.
    const auto seedBegin = Clock::now();
    if (!m_runner->seedTransportReplicationBaseline(
            sessionId,
            bootstrapSnapshot))
    {
        return false;
    }

    const double seedMs = std::chrono::duration<double, std::milli>(
        Clock::now() - seedBegin
    ).count();

    // The first authoritative snapshot for each connection is bootstrap data,
    // not a latency-simulated gameplay packet. Its session navigation view is
    // already composed from that connection's controlled entity.
    const auto sendBegin = Clock::now();
    transport.publishSnapshotImmediately(bootstrapSnapshot);
    const double sendMs = std::chrono::duration<double, std::milli>(
        Clock::now() - sendBegin
    ).count();

    std::cerr << "[M8E-CONNECT][server] bootstrap-detail session="
              << sessionId.value
              << " welcome_ms=" << welcomeMs
              << " copy_ms=" << copyMs
              << " seed_ms=" << seedMs
              << " queue_snapshot_ms=" << sendMs
              << " ships=" << bootstrapSnapshot.ships.size()
              << " objects=" << bootstrapSnapshot.objects.size()
              << " hubs=" << bootstrapSnapshot.hubs.size()
              << "\n";
    return true;
}

PlayerId ServerRuntime::resolveOrBindAccount(
    const game::network::SessionHello& hello
)
{
    if (!hello.authToken.valid())
        return {};

    // Hash immediately at the server admission boundary. The raw bearer token
    // exists only in the received SessionHello and is never retained in
    // AccountRegistry or authoritative world state.
    const auto credentialDigest =
        game::identity::authTokenDigest(hello.authToken);
    if (!credentialDigest.valid())
    {
        std::cerr << "[M8E-CONNECT][server] auth rejected: invalid digest\n";
        return {};
    }

    AccountId resolvedAccount {};
    PlayerId resolvedPlayer {};
    const auto result = m_accounts.resolve(
        credentialDigest,
        resolvedAccount,
        resolvedPlayer
    );

    if (result == game::server::AccountRegistry::ResolveResult::Bound)
    {
        std::cerr << "[M8E-CONNECT][server] auth resolved existing player="
                  << resolvedPlayer.value << "\n";
        return resolvedPlayer;
    }

    // First contact with an unknown strong token enrolls one unbound bootstrap
    // player. AccountId is created by the server and never supplied by the
    // client. This is intentionally the only first-contact shortcut; once the
    // binding exists, reconnecting with the same token resolves the same
    // PlayerId and therefore the same persistent ShipInstanceId.
    for (const PlayerId candidate : m_server->playerIdentities())
    {
        if (!candidate || m_accounts.isPlayerBound(candidate))
            continue;

        AccountId accountId {m_nextAccountId++};
        if (!accountId)
            accountId = AccountId{m_nextAccountId++};

        if (m_accounts.bind(credentialDigest, accountId, candidate))
        {
            std::cerr << "[M8E-CONNECT][server] auth enrolled account="
                      << accountId.value
                      << " player=" << candidate.value << "\n";
            return candidate;
        }
    }

    std::cerr << "[M8E-CONNECT][server] auth rejected: "
              << "no unbound bootstrap player slot\n";
    return {};
}

game::network::ServerSessionId
ServerRuntime::attachResolvedPlayerSessionTransport(
    IServerTransport& transport,
    PlayerId playerId
)
{
    using Clock = std::chrono::steady_clock;

    const auto createBegin = Clock::now();
    const auto sessionId =
        m_server->createPlayerSession(playerId);
    const double createMs = std::chrono::duration<double, std::milli>(
        Clock::now() - createBegin
    ).count();

    if (!sessionId)
    {
        std::cerr << "[M8E-CONNECT][server] session rejected player="
                  << playerId.value
                  << " reason=already-active-or-invalid"
                  << " create_ms=" << createMs << "\n";
        return {};
    }

    std::cerr << "[M8E-CONNECT][server] session created player="
              << playerId.value
              << " session=" << sessionId.value
              << " create_ms=" << createMs << "\n";

    if (!m_runner->attachTransport(transport, sessionId))
    {
        m_server->disconnectPlayerSession(sessionId);
        return {};
    }

    const auto bootstrapBegin = Clock::now();
    if (!publishSessionBootstrap(transport, sessionId))
    {
        m_runner->detachTransport(sessionId);
        m_server->disconnectPlayerSession(sessionId);
        std::cerr << "[M8E-CONNECT][server] bootstrap failed session="
                  << sessionId.value << "\n";
        return {};
    }
    const double bootstrapMs = std::chrono::duration<double, std::milli>(
        Clock::now() - bootstrapBegin
    ).count();

    std::cerr << "[M8E-CONNECT][server] bootstrap queued session="
              << sessionId.value
              << " duration_ms=" << bootstrapMs
              << " thread=" << std::this_thread::get_id() << "\n";

    return sessionId;
}

game::network::ServerSessionId
ServerRuntime::attachPlayerSessionTransport(
    IServerTransport& transport,
    const game::network::SessionHello& hello
)
{
    const PlayerId playerId = resolveOrBindAccount(hello);
    if (!playerId)
        return {};

    return attachResolvedPlayerSessionTransport(transport, playerId);
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
