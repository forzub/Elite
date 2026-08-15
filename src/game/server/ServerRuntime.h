#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

#include "src/game/server/ServerRunner.h"
#include "src/game/server/AccountRegistry.h"
#include "src/game/network/SessionMessage.h"
#include "src/game/identity/PlayerId.h"
#include "src/world/WorldParams.h"
#include "src/scene/EntityID.h"

class GameServer;
class IServerTransport;

namespace game::debug
{
class IServerDebugChannel;
struct DebugCommand;
struct DebugSessionState;
}

namespace game::server
{
/*
    Sole owner of an authoritative GameServer inside one server execution
    context.

    Local in-process play constructs/advances/destroys this runtime exclusively
    on ServerWorker's OS thread. The standalone EliteServer process owns the
    same runtime directly on its server main thread. Gameplay transport and
    debug/control cross explicit message/value seams; application/client code
    cannot reach authoritative memory directly.
*/
class ServerRuntime final
{
public:
    // Dedicated/network runtime starts without a synthetic primary gameplay
    // connection. Real accepted transports are admitted later.
    ServerRuntime(
        const WorldParams& worldParams,
        game::debug::IServerDebugChannel& debugChannel
    );

    ServerRuntime(
        const WorldParams& worldParams,
        IServerTransport& transport,
        game::debug::IServerDebugChannel& debugChannel
    );
    ~ServerRuntime();

    ServerRuntime(const ServerRuntime&) = delete;
    ServerRuntime& operator=(const ServerRuntime&) = delete;

    ServerAdvanceResult advance(double elapsedSeconds);
    double fixedStepSeconds() const;

    // Production-shaped admission is token-authenticated. The client presents
    // only an opaque bearer token; AccountId / PlayerId / ShipInstanceId /
    // EntityId are resolved entirely by authoritative server registries.
    game::network::ServerSessionId attachPlayerSessionTransport(
        IServerTransport& transport,
        const game::network::SessionHello& hello
    );
    bool detachPlayerSessionTransport(
        game::network::ServerSessionId sessionId
    );
    std::size_t connectedPlayerSessionCount() const noexcept;

private:
    ServerRuntime(
        const WorldParams& worldParams,
        game::debug::IServerDebugChannel& debugChannel,
        std::size_t bootstrapPlayerSlotCount
    );

    game::network::ServerSessionId attachResolvedPlayerSessionTransport(
        IServerTransport& transport,
        PlayerId playerId
    );
    PlayerId resolveOrBindAccount(
        const game::network::SessionHello& hello
    );
    bool publishSessionBootstrap(
        IServerTransport& transport,
        game::network::ServerSessionId sessionId
    );
    void receiveDebugCommands();
    void publishPendingDebugSnapshot();
    game::debug::DebugSessionState makeDebugState() const;

    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<ServerRunner> m_runner;
    game::server::AccountRegistry m_accounts;
    std::uint64_t m_nextAccountId = 1;
    game::network::ServerSessionId m_primarySessionId {};
    game::debug::IServerDebugChannel& m_debugChannel;

    bool m_debugSnapshotPending = false;
    std::uint64_t m_debugSnapshotBaseServerTick = 0;
    bool m_debugStateDirty = false;
};
}
