#pragma once

#include <memory>
#include <vector>

class GameState;

class StateStack
{
public:
    StateStack();

    void push(std::unique_ptr<GameState> state);
    void pop();
    void clear();

    void applyPendingChanges();

    GameState* current();
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

    std::vector<std::unique_ptr<GameState>> m_stack;
    std::vector<PendingAction> m_pending;
};
