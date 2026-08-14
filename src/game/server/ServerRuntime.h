#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

#include "src/game/server/ServerRunner.h"
#include "src/game/network/SessionMessage.h"
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

    // Production admission is server-owned: callers provide a connection,
    // never an EntityId. The explicit-entity overload remains for deterministic
    // architecture/diagnostic tests and future authenticated handoff code.
    game::network::ServerSessionId attachPlayerSessionTransport(
        IServerTransport& transport
    );
    game::network::ServerSessionId attachPlayerSessionTransport(
        IServerTransport& transport,
        EntityId controlledEntityId
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

    bool publishSessionBootstrap(
        IServerTransport& transport,
        game::network::ServerSessionId sessionId
    );
    void receiveDebugCommands();
    void publishPendingDebugSnapshot();
    game::debug::DebugSessionState makeDebugState() const;

    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<ServerRunner> m_runner;
    game::network::ServerSessionId m_primarySessionId {};
    game::debug::IServerDebugChannel& m_debugChannel;

    bool m_debugSnapshotPending = false;
    std::uint64_t m_debugSnapshotBaseServerTick = 0;
    bool m_debugStateDirty = false;
};
}
