#include "src/game/host/LocalGameSession.h"

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
}

LocalGameSession::~LocalGameSession() = default;

void LocalGameSession::beginSynchronization()
{
    m_client->beginSynchronization();
}

void LocalGameSession::updateSynchronization(double elapsedSeconds)
{
    if (state() != game::session::GameSessionState::Synchronizing)
        return;

    m_host->advance(elapsedSeconds);
    m_client->updateSynchronization(
        static_cast<float>(elapsedSeconds)
    );
}

game::session::GameSessionState LocalGameSession::state() const
{
    switch (m_client->connectionState())
    {
        case ClientConnectionState::Connecting:
            return game::session::GameSessionState::Created;
        case ClientConnectionState::Synchronizing:
            return game::session::GameSessionState::Synchronizing;
        case ClientConnectionState::Ready:
            return game::session::GameSessionState::Ready;
        case ClientConnectionState::Failed:
            return game::session::GameSessionState::Failed;
        case ClientConnectionState::Disconnected:
        default:
            return game::session::GameSessionState::Created;
    }
}

const std::string& LocalGameSession::error() const
{
    return m_client->connectionError();
}


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
