#pragma once

#include "src/scene/EntityID.h"

class GameClient;

namespace game::debug
{
class IDebugSessionControl;
}

namespace game::session
{
struct GameSessionAdvanceResult
{
    int stepsExecuted = 0;
    double remainingDebtSeconds = 0.0;
    double discardedSeconds = 0.0;
    double totalDiscardedSeconds = 0.0;
    bool catchUpLimited = false;
};

class IGameSession
{
public:
    virtual ~IGameSession() = default;

    virtual GameClient& client() = 0;
    virtual const GameClient& client() const = 0;

    virtual game::debug::IDebugSessionControl* debugControl() = 0;
    virtual const game::debug::IDebugSessionControl* debugControl() const = 0;

    virtual EntityId playerId() const = 0;

    virtual GameSessionAdvanceResult advance(double elapsedSeconds) = 0;
    virtual double fixedStepSeconds() const = 0;

    virtual void configureWorld(float linearDrag, float maxSafeDecel) = 0;
};
}
