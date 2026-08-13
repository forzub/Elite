#pragma once

#include <cstdint>
#include <memory>

#include "src/game/server/ServerRunner.h"
#include "src/world/WorldParams.h"

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

private:
    void receiveDebugCommands();
    void publishPendingDebugSnapshot();
    game::debug::DebugSessionState makeDebugState() const;

    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<ServerRunner> m_runner;
    game::debug::IServerDebugChannel& m_debugChannel;

    bool m_debugSnapshotPending = false;
    std::uint64_t m_debugSnapshotBaseServerTick = 0;
    bool m_debugStateDirty = false;
};
}
