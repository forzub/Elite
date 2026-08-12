#pragma once

#include <string>

#include "src/scene/EntityID.h"

class GameClient;

namespace game::debug
{
class IDebugSessionControl;
}

namespace game::session
{
enum class GameSessionState
{
    Created,
    Synchronizing,
    Ready,
    Failed
};

struct GameSessionAdvanceResult
{
    int stepsExecuted = 0;
    double remainingDebtSeconds = 0.0;
    double discardedSeconds = 0.0;
    double totalDiscardedSeconds = 0.0;
    bool catchUpLimited = false;
    double serverExecutionWallSeconds = 0.0;
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

    virtual void beginSynchronization() = 0;
    virtual void updateSynchronization(double elapsedSeconds) = 0;
    virtual GameSessionState state() const = 0;
    virtual const std::string& error() const = 0;

    virtual GameSessionAdvanceResult advance(double elapsedSeconds) = 0;
    virtual double fixedStepSeconds() const = 0;

};
}
