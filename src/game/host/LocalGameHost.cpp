#include "src/game/host/LocalGameHost.h"

#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/debug/LocalDebugSessionControl.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/server/ServerWorker.h"

namespace game::host
{
LocalGameHost::LocalGameHost(
    const WorldParams& worldParams,
    const game::network::SessionHello& identityHello
)
    : m_transport(std::make_unique<LocalLoopbackTransport>())
    , m_debugControl(std::make_unique<game::debug::LocalDebugSessionControl>())
{
    // Queue the same account-authentication message used by a TCP client before
    // the authoritative worker constructs ServerRuntime. Local play therefore
    // has no special PlayerId shortcut.
    m_transport->sendSessionHello(identityHello);

    m_worker = std::make_unique<server::ServerWorker>(
        worldParams,
        *m_transport,
        *m_debugControl
    );
}

LocalGameHost::~LocalGameHost() = default;

ITransport& LocalGameHost::transport()
{
    return *m_transport;
}

const ITransport& LocalGameHost::transport() const
{
    return *m_transport;
}

game::debug::IDebugSessionControl& LocalGameHost::debugControl()
{
    return *m_debugControl;
}

const game::debug::IDebugSessionControl& LocalGameHost::debugControl() const
{
    return *m_debugControl;
}

server::ServerAdvanceResult LocalGameHost::advance(double elapsedSeconds)
{
    return m_worker->advance(elapsedSeconds);
}

double LocalGameHost::fixedStepSeconds() const
{
    return m_worker->fixedStepSeconds();
}
}
