#include "src/game/host/LocalGameSession.h"

#include <stdexcept>

#include "src/game/client/GameClient.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/host/LocalGameHost.h"

namespace game::host
{
LocalGameSession::LocalGameSession(
    const LocalGameSessionConfig& config
)
    : m_host(std::make_unique<LocalGameHost>(config.world))
    , m_client(std::make_unique<GameClient>(
          m_host->transport(),
          m_host->playerId()
      ))
{
    m_client->beginSynchronization();

    // Catalog and celestial requests are authoritative server requests.
    // Advance one fixed tick so the local server can process them before the
    // client evaluates startup readiness.
    m_host->advance(m_host->fixedStepSeconds());

    m_client->update(
        0.0f,
        static_cast<float>(m_host->fixedStepSeconds())
    );

    if (!m_client->readyForGameplay())
    {
        m_client->failSynchronization(
            "Local session did not provide the complete startup state"
        );
        throw std::runtime_error(m_client->connectionError());
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

game::debug::IDebugSessionControl* LocalGameSession::debugControl()
{
    return m_host.get();
}

const game::debug::IDebugSessionControl* LocalGameSession::debugControl() const
{
    return m_host.get();
}

EntityId LocalGameSession::playerId() const
{
    return m_host->playerId();
}

game::session::GameSessionAdvanceResult LocalGameSession::advance(
    double elapsedSeconds
)
{
    const auto result = m_host->advance(elapsedSeconds);

    game::session::GameSessionAdvanceResult sessionResult;
    sessionResult.stepsExecuted = result.stepsExecuted;
    sessionResult.remainingDebtSeconds = result.remainingDebtSeconds;
    sessionResult.discardedSeconds = result.discardedSeconds;
    sessionResult.totalDiscardedSeconds = result.totalDiscardedSeconds;
    sessionResult.catchUpLimited = result.catchUpLimited;
    return sessionResult;
}

double LocalGameSession::fixedStepSeconds() const
{
    return m_host->fixedStepSeconds();
}
}
