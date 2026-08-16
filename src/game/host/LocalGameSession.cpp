#include "src/game/host/LocalGameSession.h"

#include <exception>

#include "src/game/client/GameClient.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/host/LocalGameHost.h"

namespace game::host
{
LocalGameSession::LocalGameSession(
    const LocalGameSessionConfig& config
)
    : m_host(std::make_unique<LocalGameHost>(
          config.world,
          config.identityHello
      ))
    , m_client(std::make_unique<GameClient>(m_host->transport()))
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

    try
    {
        // ServerRuntime may spend seconds loading the initial authoritative
        // world. That work already lives on ServerWorker; do not defeat the
        // thread boundary by synchronously waiting for it from the UI thread.
        m_host->rethrowIfFailed();
        if (!m_host->ready())
            return;

        m_host->advance(elapsedSeconds);
        m_client->updateSynchronization(elapsedSeconds);
    }
    catch (const std::exception& ex)
    {
        m_client->failSynchronization(
            std::string("Local authoritative startup failed: ") + ex.what()
        );
    }
    catch (...)
    {
        m_client->failSynchronization(
            "Local authoritative startup failed"
        );
    }
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
    return &m_host->debugControl();
}

const game::debug::IDebugSessionControl* LocalGameSession::debugControl() const
{
    const LocalGameHost& host = *m_host;
    return &host.debugControl();
}

EntityId LocalGameSession::playerId() const
{
    return m_client->playerId();
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
    sessionResult.serverExecutionWallSeconds = result.executionWallSeconds;
    return sessionResult;
}

double LocalGameSession::fixedStepSeconds() const
{
    return m_host->fixedStepSeconds();
}
}
