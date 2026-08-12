#pragma once

#include <memory>

#include "src/game/server/ServerRunner.h"
#include "src/world/WorldParams.h"

class ITransport;
class LocalLoopbackTransport;

namespace game::debug
{
class IDebugSessionControl;
}

namespace game::server
{
class ServerRuntime;
}

namespace game::host
{
/*
    Owns the in-process connection and authoritative runtime as separate peers.

    The host never owns or exposes GameServer. ServerRuntime is the sole owner
    of authoritative memory; client-facing code receives only ITransport and
    the explicit debug facade. This keeps the composition layer ready for the
    runtime to move to another thread/process later.
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
    std::unique_ptr<server::ServerRuntime> m_runtime;
};
}
