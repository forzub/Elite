#include "src/game/session/RemoteGameSession.h"

#include <algorithm>
#include <utility>

#include "src/game/client/GameClient.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/network/TcpTransport.h"

namespace game::session
{
struct RemoteGameSession::NullDebugSessionControl final
    : public game::debug::IDebugSessionControl
{
    SimulationSnapshot snapshot() const override { return {}; }
    std::uint64_t snapshotRevision() const override { return 0; }
    std::uint64_t stateRevision() const override { return 0; }
    void refreshSnapshot() override {}
    void refreshStructureSnapshot() override {}
    void destroyShipModule(EntityId, const std::string&) override {}
    void restoreShipModule(EntityId, const std::string&) override {}
    void resetShipStructure(EntityId) override {}
    void resetAllShipStructures() override {}
    void detachShipModule(EntityId, const std::string&) override {}
    void hangShipModule(EntityId, const std::string&) override {}
    void reevaluateShipStructure(EntityId) override {}
    void setShipStructuralLinkHealth(
        EntityId,
        const std::string&,
        float,
        bool) override {}
    bool fastUniverseTime() const override { return false; }
    bool universeTimeSimulation() const override { return false; }
    double universeTimeScale() const override { return 1.0; }
    double configuredUniverseTimeScale() const override { return 1.0; }
    void setUniverseTimeSimulation(bool, double) override {}
};

RemoteGameSession::RemoteGameSession(RemoteGameSessionConfig config)
    : m_config(std::move(config))
    , m_transport(std::make_unique<game::network::TcpClientTransport>())
    , m_client(std::make_unique<GameClient>(*m_transport))
    , m_debugControl(std::make_unique<NullDebugSessionControl>())
{
}

RemoteGameSession::~RemoteGameSession()
{
    if (m_transport)
        m_transport->disconnect();
}

GameClient& RemoteGameSession::client()
{
    return *m_client;
}

const GameClient& RemoteGameSession::client() const
{
    return *m_client;
}

game::debug::IDebugSessionControl* RemoteGameSession::debugControl()
{
    return m_debugControl.get();
}

const game::debug::IDebugSessionControl* RemoteGameSession::debugControl() const
{
    return m_debugControl.get();
}

EntityId RemoteGameSession::playerId() const
{
    return m_client->playerId();
}

void RemoteGameSession::beginSynchronization()
{
    m_error.clear();
    m_retryElapsedSeconds = 0.0;
    m_waitingForServer = false;
    m_connectedOnce = false;
    m_failed = false;
    m_started = true;

    (void)connectOrWait();
}

bool RemoteGameSession::connectOrWait()
{
    if (m_transport->connect(m_config.host, m_config.port))
    {
        if (!m_config.identityHello.authToken.valid())
        {
            m_transport->disconnect();
            m_failed = true;
            m_error = "client authentication token is invalid";
            return false;
        }

        m_transport->sendSessionHello(m_config.identityHello);
        m_error.clear();
        m_waitingForServer = false;
        m_connectedOnce = true;
        m_retryElapsedSeconds = 0.0;
        m_client->beginSynchronization();
        return true;
    }

    // Initial connection refusal/absence is not a fatal session error. A
    // remote client may legitimately start before the authoritative server.
    // Protocol/catalog/admission failures happen only after TCP has connected
    // and remain fatal through the normal GameClient/transport paths.
    m_waitingForServer = true;
    m_retryElapsedSeconds = 0.0;
    m_error = m_transport->lastError();
    if (m_error.empty())
        m_error = "authoritative server is not available yet";
    return false;
}

void RemoteGameSession::updateSynchronization(double elapsedSeconds)
{
    if (!m_started || m_failed)
        return;

    const double dt = std::max(0.0, elapsedSeconds);

    if (m_waitingForServer)
    {
        m_retryElapsedSeconds += dt;
        const double retryInterval =
            std::max(0.10, m_config.retryIntervalSeconds);

        if (m_retryElapsedSeconds >= retryInterval)
            (void)connectOrWait();

        return;
    }

    m_transport->service();
    captureTransportFailure();
    if (m_failed)
        return;

    (void)m_client->updateSynchronization(dt);

    if (m_client->connectionState() == ClientConnectionState::Failed)
    {
        m_failed = true;
        m_error = m_client->connectionError();
    }
}

GameSessionState RemoteGameSession::state() const
{
    if (m_failed)
        return GameSessionState::Failed;
    if (!m_started)
        return GameSessionState::Created;
    if (m_waitingForServer)
        return GameSessionState::WaitingForServer;

    switch (m_client->connectionState())
    {
        case ClientConnectionState::Ready:
            return GameSessionState::Ready;
        case ClientConnectionState::Failed:
            return GameSessionState::Failed;
        case ClientConnectionState::Synchronizing:
            return GameSessionState::Synchronizing;
        case ClientConnectionState::Connecting:
        case ClientConnectionState::Disconnected:
        default:
            return GameSessionState::Created;
    }
}

const std::string& RemoteGameSession::error() const
{
    return m_error.empty() ? m_client->connectionError() : m_error;
}

GameSessionAdvanceResult RemoteGameSession::advance(double)
{
    GameSessionAdvanceResult result;
    if (!m_started || m_failed)
        return result;

    // Remote authoritative time advances in EliteServer. This call only pumps
    // transport completion before GameClient performs prediction/interpolation.
    m_transport->service();
    captureTransportFailure();
    return result;
}

double RemoteGameSession::fixedStepSeconds() const
{
    const double serverStep = m_client->serverFixedStepSeconds();
    return serverStep > 0.0 ? serverStep : 0.02;
}

void RemoteGameSession::captureTransportFailure()
{
    if (!m_started || !m_transport || m_transport->connected())
        return;

    // Before the first successful TCP connection the session is deliberately
    // retryable. Once a server has accepted us, a later disconnect is a real
    // lifecycle failure until explicit reconnect/resume semantics are added.
    if (!m_connectedOnce)
    {
        m_waitingForServer = true;
        return;
    }

    m_failed = true;
    m_error = m_transport->lastError();
    if (m_error.empty())
        m_error = "authoritative server disconnected";
}
}
