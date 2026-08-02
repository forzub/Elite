#include "src/game/host/LocalGameSession.h"

#include <stdexcept>

#include "src/game/client/GameClient.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/host/LocalGameHost.h"

namespace game::host
{
LocalGameSession::LocalGameSession()
    : m_host(std::make_unique<LocalGameHost>())
    , m_client(std::make_unique<GameClient>(
          &m_host->transport(),
          m_host->playerId()
      ))
{
    m_client->update(
        0.0f,
        static_cast<float>(m_host->fixedStepSeconds())
    );

    if (!m_client->requestStarAtlas() ||
        !m_client->requestCelestialSnapshot())
    {
        throw std::runtime_error(
            "Local session catalog handshake is incomplete"
        );
    }

    if (!m_client->readyForGameplay())
    {
        throw std::runtime_error(
            "Local session gameplay handshake is incomplete"
        );
    }
}

LocalGameSession::~LocalGameSession() = default;

GameClient& LocalGameSession::client()
{
    return *m_client;
}

const GameClient& LocalGameSession::client() const
{
    return *m_client;
}

game::debug::IDebugSessionControl& LocalGameSession::debugControl()
{
    return *m_host;
}

const game::debug::IDebugSessionControl& LocalGameSession::debugControl() const
{
    return *m_host;
}

EntityId LocalGameSession::playerId() const
{
    return m_host->playerId();
}

server::ServerAdvanceResult LocalGameSession::advance(
    double elapsedSeconds
)
{
    return m_host->advance(elapsedSeconds);
}

double LocalGameSession::fixedStepSeconds() const
{
    return m_host->fixedStepSeconds();
}

void LocalGameSession::configureWorld(
    float linearDrag,
    float maxSafeDecel
)
{
    m_host->configureWorld(linearDrag, maxSafeDecel);
}
}
