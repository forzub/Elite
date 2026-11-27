#include "src/model_asset/HubExpansion.h"
#include "src/game/server/ServerRuntime.h"
#include "src/core/RuntimeTrace.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "src/game/economy/EconomyServer.h"
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

namespace
{
// DEV: ELITE_DEV_PLAYER_SLOTS=<n> (the "2 slots -> 1 -> 2" ownership persistence check: an owner player absent from
// the universe keeps its exact PlayerId on disk and gets its ship back when the slot exists again)
std::size_t devPlayerSlots(std::size_t requested)
{
    const char* v = std::getenv("ELITE_DEV_PLAYER_SLOTS");
    const int n = v ? std::atoi(v) : 0;
    return n > 0 && n <= 8 ? static_cast<std::size_t>(n) : requested;
}
} // namespace

ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    game::debug::IServerDebugChannel& debugChannel,
    std::size_t bootstrapPlayerSlotCount
)
    : m_server(std::make_unique<GameServer>(devPlayerSlots(bootstrapPlayerSlotCount)))
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
    // Local play uses the same explicit account identity seam as a remote
    // client. Application marks the private-runtime hello as Register before
    // ServerWorker constructs the authoritative runtime. No local-only
    // PlayerId shortcut.
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

bool ServerRuntime::devPlaceShipInSystem(EntityId shipId, int systemId, int ordinal)
{
    return m_server->devPlaceShipInSystem(shipId, systemId, ordinal);
}

std::size_t ServerRuntime::lastTickSystemPasses() const noexcept
{
    return m_server->lastTickSystemPasses();
}

long long ServerRuntime::contextViolations() const
{
    return m_server->contextViolations();
}

double ServerRuntime::lastTickSimulationSeconds() const noexcept
{
    return m_server->lastTickSimulationSeconds();
}

std::size_t ServerRuntime::orbitalHubCount() const
{
    return m_server->orbitalHubCount();
}

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

game::diagnostics::NavigationRuntimeLabObservation
ServerRuntime::navigationRuntimeLabObservation() const noexcept
{
    return m_server->navigationRuntimeLabObservation();
}

bool ServerRuntime::copyAuthoritativePublishedSnapshot(
    SimulationSnapshot& outSnapshot
) const
{
    if (!m_server)
        return false;

    outSnapshot = m_server->snapshot();
    return true;
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
    welcome.hubContentFingerprint = elite::model_asset::hubContentFingerprint();
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

    if (core::runtimeTraceEnabled())
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

PlayerId ServerRuntime::resolveOrRegisterAccount(
    const game::network::SessionHello& hello,
    game::network::SessionReject& outReject
)
{
    outReject = {};

    if (!game::identity::isValidAccountHandle(hello.accountHandle))
    {
        outReject.reason = game::network::SessionRejectReason::InvalidAccountHandle;
        return {};
    }

    if (!hello.authToken.valid())
    {
        outReject.reason = game::network::SessionRejectReason::InvalidCredential;
        return {};
    }

    // Hash immediately at the authoritative admission boundary. Raw bearer
    // tokens never enter AccountRegistry or world state.
    const auto credentialDigest =
        game::identity::authTokenDigest(hello.authToken);
    if (!credentialDigest.valid())
    {
        outReject.reason = game::network::SessionRejectReason::InvalidCredential;
        return {};
    }

    AccountId resolvedAccount {};
    PlayerId resolvedPlayer {};
    const auto result = m_accounts.resolve(
        hello.accountHandle,
        credentialDigest,
        resolvedAccount,
        resolvedPlayer
    );

    if (result == game::server::AccountRegistry::ResolveResult::Bound)
    {
        if (core::runtimeTraceEnabled())
            std::cerr << "[M8E-AUTH][server] sign-in accepted handle="
                      << hello.accountHandle
                      << " account=" << resolvedAccount.value
                      << " player=" << resolvedPlayer.value << "\n";
        return resolvedPlayer;
    }

    if (hello.intent != game::network::AuthenticationIntent::Register)
    {
        outReject.reason =
            result == game::server::AccountRegistry::ResolveResult::InvalidCredential
                ? game::network::SessionRejectReason::InvalidCredential
                : game::network::SessionRejectReason::UnknownAccount;
        std::cerr << "[M8E-AUTH][server] sign-in rejected handle="
                  << hello.accountHandle
                  << " reason="
                  << game::network::sessionRejectCode(outReject.reason)
                  << "\n";
        return {};
    }

    if (result == game::server::AccountRegistry::ResolveResult::InvalidCredential)
    {
        outReject.reason = game::network::SessionRejectReason::AccountHandleTaken;
        std::cerr << "[M8E-AUTH][server] registration rejected handle="
                  << hello.accountHandle
                  << " reason="
                  << game::network::sessionRejectCode(outReject.reason)
                  << "\n";
        return {};
    }

    // Registration is explicit. It may allocate only from authoritative
    // bootstrap player records and the client never selects a PlayerId.
    for (const PlayerId candidate : m_server->playerIdentities())
    {
        if (!candidate || m_accounts.isPlayerBound(candidate))
            continue;

        AccountId accountId {m_nextAccountId++};
        if (!accountId)
            accountId = AccountId{m_nextAccountId++};

        if (m_accounts.bind(
                hello.accountHandle,
                credentialDigest,
                accountId,
                candidate))
        {
            saveAccounts();
            if (core::runtimeTraceEnabled())
                std::cerr << "[M8E-AUTH][server] registration accepted handle="
                          << hello.accountHandle
                          << " account=" << accountId.value
                          << " player=" << candidate.value << "\n";
            return candidate;
        }
    }

    outReject.reason =
        game::network::SessionRejectReason::RegistrationUnavailable;
    std::cerr << "[M8E-AUTH][server] registration rejected reason="
              << game::network::sessionRejectCode(outReject.reason)
              << "\n";
    return {};
}

game::network::ServerSessionId
ServerRuntime::attachResolvedPlayerSessionTransport(
    IServerTransport& transport,
    PlayerId playerId,
    game::network::SessionReject& outReject
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
        outReject.reason = game::network::SessionRejectReason::AlreadyActive;
        std::cerr << "[M8E-CONNECT][server] session rejected player="
                  << playerId.value
                  << " reason=" << game::network::sessionRejectCode(outReject.reason)
                  << " create_ms=" << createMs << "\n";
        return {};
    }

    if (core::runtimeTraceEnabled())
        std::cerr << "[M8E-CONNECT][server] session created player="
                  << playerId.value
                  << " session=" << sessionId.value
                  << " create_ms=" << createMs << "\n";

    if (!m_runner->attachTransport(transport, sessionId))
    {
        m_server->disconnectPlayerSession(sessionId);
        outReject.reason = game::network::SessionRejectReason::SessionUnavailable;
        outReject.retryable = false;
        return {};
    }

    const auto bootstrapBegin = Clock::now();
    if (!publishSessionBootstrap(transport, sessionId))
    {
        m_runner->detachTransport(sessionId);
        m_server->disconnectPlayerSession(sessionId);
        outReject.reason = game::network::SessionRejectReason::BootstrapFailed;
        outReject.retryable = false;
        std::cerr << "[M8E-CONNECT][server] bootstrap failed session="
                  << sessionId.value << "\n";
        return {};
    }
    const double bootstrapMs = std::chrono::duration<double, std::milli>(
        Clock::now() - bootstrapBegin
    ).count();

    if (core::runtimeTraceEnabled())
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
    game::network::SessionReject reject;
    const PlayerId playerId = resolveOrRegisterAccount(hello, reject);
    if (!playerId)
    {
        transport.publishSessionRejectImmediately(reject);
        return {};
    }

    const auto sessionId =
        attachResolvedPlayerSessionTransport(transport, playerId, reject);
    if (!sessionId)
        transport.publishSessionRejectImmediately(reject);
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

bool ServerRuntime::resetAuthenticationStateForDevelopment()
{
    if (connectedPlayerSessionCount() != 0u)
        return false;

    m_accounts.reset();
    m_nextAccountId = 1;
    saveAccounts();
    return true;
}

void ServerRuntime::enableAccountPersistence(bool startEmpty)
{
    const std::string state = m_server->persistentStatePath();
    if (state.empty())
    {
        std::cerr << "[Accounts] no server state file (ELITE_ECONOMY_STATE_FILE): accounts live in memory only" << std::endl;
        return;
    }
    m_accountsFile = state + ".accounts.json";
    if (startEmpty)
    {
        saveAccounts();
        return;
    }
    std::ifstream f(m_accountsFile, std::ios::binary);
    if (!f)
        return;
    nlohmann::json j;
    try
    {
        f >> j;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Accounts] " << m_accountsFile << " unreadable: " << e.what() << std::endl;
        m_accountsFile.clear();   // never overwrite a file we could not read
        return;
    }
    if (j.value("schema", std::string()) != "accounts/1")
        return;
    m_nextAccountId = std::max(m_nextAccountId, j.value("next_account", std::uint64_t{1}));
    const auto identities = m_server->playerIdentities();
    std::size_t restored = 0;
    for (const auto& a : j.value("accounts", nlohmann::json::array()))
    {
        game::identity::AuthTokenDigest digest;
        const std::string hex = a.value("digest", std::string());
        if (hex.size() != digest.bytes.size() * 2)
            continue;
        for (std::size_t i = 0; i < digest.bytes.size(); ++i)
            digest.bytes[i] = static_cast<std::uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
        const PlayerId player {a.value("player", std::uint64_t{0})};
        if (std::find(identities.begin(), identities.end(), player) == identities.end())
        {
            std::cerr << "[Accounts] account '" << a.value("handle", std::string()) << "' -> player " << player.value
                      << ": no such player identity in this universe - not bound (kept in the file)" << std::endl;
            m_unboundAccounts.push_back(a);
            continue;
        }
        if (m_accounts.bind(a.value("handle", std::string()), digest, AccountId{a.value("account", std::uint64_t{0})}, player))
            ++restored;
    }
    std::cerr << "[Accounts] restored " << restored << " account(s) from " << m_accountsFile << std::endl;
}

void ServerRuntime::saveAccounts() const
{
    if (m_accountsFile.empty())
        return;
    nlohmann::json j;
    j["schema"] = "accounts/1";
    j["next_account"] = m_nextAccountId;
    j["accounts"] = nlohmann::json::array();
    for (const auto& [digest, record] : m_accounts.all())
    {
        std::string hex;
        char b[3];
        for (const auto byte : digest.bytes)
        {
            std::snprintf(b, sizeof(b), "%02x", static_cast<unsigned>(byte));
            hex += b;
        }
        j["accounts"].push_back({{"handle", record.accountHandle}, {"digest", hex}, {"account", record.accountId.value},
                                 {"player", record.playerId.value}});
    }
    for (const auto& a : m_unboundAccounts)
        j["accounts"].push_back(a);
    if (!game::economy::writeFileAtomically(m_accountsFile, j.dump(1)))
        std::cerr << "[Accounts] NOT saved: " << m_accountsFile << std::endl;
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
