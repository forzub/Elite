#pragma once

#include <memory>

#include "src/scene/EntityID.h"
#include "src/game/server/ServerRunner.h"

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
class LocalGameSession final
{
public:
    LocalGameSession();
    ~LocalGameSession();

    LocalGameSession(const LocalGameSession&) = delete;
    LocalGameSession& operator=(const LocalGameSession&) = delete;

    GameClient& client();
    const GameClient& client() const;

    game::debug::IDebugSessionControl& debugControl();
    const game::debug::IDebugSessionControl& debugControl() const;

    EntityId playerId() const;

    server::ServerAdvanceResult advance(double elapsedSeconds);
    double fixedStepSeconds() const;

    void configureWorld(float linearDrag, float maxSafeDecel);

private:
    std::unique_ptr<LocalGameHost> m_host;
    std::unique_ptr<GameClient> m_client;
};
}
