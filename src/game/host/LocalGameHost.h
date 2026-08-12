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
class ServerRuntime;
}

namespace game::host
{
/*
    Owns the in-process connection and authoritative runtime as separate peers.

    The host never owns or exposes GameServer. Gameplay transport and debug
    control each have client/tool and server endpoints, while ServerRuntime is
    the sole owner of authoritative memory. This is the composition shape that
    can later move the runtime to another thread/process without changing UI or
    client code.
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
    std::unique_ptr<server::ServerRuntime> m_runtime;
};
}
