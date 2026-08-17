#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "src/game/session/IGameSession.h"
#include "src/game/network/SessionMessage.h"

class GameClient;

namespace game::network
{
class TcpClientTransport;
}

namespace game::debug
{
class IDebugSessionControl;
}

namespace game::session
{
struct RemoteGameSessionConfig
{
    std::string host = "127.0.0.1";
    std::uint16_t port = 27351;
    double retryIntervalSeconds = 1.0;
    game::network::SessionHello identityHello {
        "remote-test",
        game::identity::AuthToken{{1u}},
        game::network::AuthenticationIntent::SignIn
    };
};

/*
    Application-side session backed by a process-remote authoritative server.

    It owns only a TcpClientTransport + GameClient. There is deliberately no
    ServerRuntime/ServerWorker here: prediction advances locally while the
    authoritative world advances in EliteServer.
*/
class RemoteGameSession final : public IGameSession
{
public:
    explicit RemoteGameSession(RemoteGameSessionConfig config);
    ~RemoteGameSession();

    RemoteGameSession(const RemoteGameSession&) = delete;
    RemoteGameSession& operator=(const RemoteGameSession&) = delete;

    GameClient& client() override;
    const GameClient& client() const override;

    game::debug::IDebugSessionControl* debugControl() override;
    const game::debug::IDebugSessionControl* debugControl() const override;

    EntityId playerId() const override;

    void beginSynchronization() override;
    void updateSynchronization(double elapsedSeconds) override;
    GameSessionState state() const override;
    const std::string& error() const override;

    GameSessionAdvanceResult advance(double elapsedSeconds) override;
    double fixedStepSeconds() const override;

private:
    bool connectOrWait();
    void captureTransportFailure();

    struct NullDebugSessionControl;

    RemoteGameSessionConfig m_config;
    std::unique_ptr<game::network::TcpClientTransport> m_transport;
    std::unique_ptr<GameClient> m_client;
    std::unique_ptr<NullDebugSessionControl> m_debugControl;
    std::string m_error;
    double m_retryElapsedSeconds = 0.0;
    bool m_started = false;
    bool m_waitingForServer = false;
    bool m_connectedOnce = false;
    bool m_failed = false;
};
}
