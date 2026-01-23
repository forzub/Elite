#pragma once

#include <memory>
#include <vector>

#include "core/StateContext.h"

class GameState;

class StateStack
{
public:
    explicit StateStack(StateContext& context);
    StateContext& context();

    void push(std::unique_ptr<GameState> state);
    void pop();
    void clear();

    void applyPendingChanges();
    void submitRenderData();

    void renderAll();


    GameState* current() const;
    bool empty() const;

private:
    enum class ActionType
    {
        Push,
        Pop,
        Clear
    };

    struct PendingAction
    {
        ActionType type;
        std::unique_ptr<GameState> state;
    };

    StateContext& m_context;

    std::vector<std::unique_ptr<GameState>> m_stack;
    std::vector<PendingAction> m_pending;
};
