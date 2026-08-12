#pragma once

#include <memory>

#include "src/game/server/ServerRunner.h"
#include "src/world/WorldParams.h"

class ITransport;
class LocalLoopbackTransport;

namespace game::debug
{
class IDebugSessionControl;
class LocalDebugSessionControl;
}

namespace game::server
{
class ServerWorker;
}

namespace game::host
{
/*
    Owns the in-process connection and the server worker as separate peers.

    The host never owns or exposes GameServer/ServerRuntime. Gameplay transport
    and debug control each have client/tool and server endpoints, while
    ServerWorker exclusively constructs, advances and destroys authoritative
    state on its OS thread.
*/
class LocalGameHost final
{
public:
    explicit LocalGameHost(const WorldParams& worldParams);
    ~LocalGameHost();

    LocalGameHost(const LocalGameHost&) = delete;
    LocalGameHost& operator=(const LocalGameHost&) = delete;

    ITransport& transport();
    const ITransport& transport() const;

    game::debug::IDebugSessionControl& debugControl();
    const game::debug::IDebugSessionControl& debugControl() const;

    server::ServerAdvanceResult advance(double elapsedSeconds);
    double fixedStepSeconds() const;

private:
    std::unique_ptr<LocalLoopbackTransport> m_transport;
    std::unique_ptr<game::debug::LocalDebugSessionControl> m_debugControl;
    std::unique_ptr<server::ServerWorker> m_worker;
};
}
