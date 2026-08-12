#include "src/game/host/LocalGameHost.h"

#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/server/ServerRuntime.h"

namespace game::host
{
LocalGameHost::LocalGameHost(const WorldParams& worldParams)
    : m_transport(std::make_unique<LocalLoopbackTransport>())
    , m_runtime(std::make_unique<server::ServerRuntime>(
          worldParams,
          *m_transport
      ))
{
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
    return *m_runtime;
}

const game::debug::IDebugSessionControl& LocalGameHost::debugControl() const
{
    return *m_runtime;
}

server::ServerAdvanceResult LocalGameHost::advance(double elapsedSeconds)
{
    return m_runtime->advance(elapsedSeconds);
}

double LocalGameHost::fixedStepSeconds() const
{
    return m_runtime->fixedStepSeconds();
}
}
