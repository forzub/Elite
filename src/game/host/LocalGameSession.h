#pragma once

#include <memory>

#include "src/game/session/IGameSession.h"

class GameClient;

namespace game::debug
{
class IDebugSessionControl;
}

namespace game::host
{
class LocalGameHost;

// Application-owned local session. Client-facing states borrow its endpoints
// but never own or construct the authoritative runtime.
class LocalGameSession final : public game::session::IGameSession
{
public:
    LocalGameSession();
    ~LocalGameSession();

    LocalGameSession(const LocalGameSession&) = delete;
    LocalGameSession& operator=(const LocalGameSession&) = delete;

    GameClient& client() override;
    const GameClient& client() const override;

    game::debug::IDebugSessionControl* debugControl() override;
    const game::debug::IDebugSessionControl* debugControl() const override;

    EntityId playerId() const override;

    game::session::GameSessionAdvanceResult advance(
        double elapsedSeconds
    ) override;
    double fixedStepSeconds() const override;

    void configureWorld(
        float linearDrag,
        float maxSafeDecel
    ) override;

private:
    std::unique_ptr<LocalGameHost> m_host;
    std::unique_ptr<GameClient> m_client;
};
}
