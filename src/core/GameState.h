#pragma once


class StateStack;   // ← ВАЖНО


class GameState
{
protected:
    StateStack& m_states;

public:
    explicit GameState(StateStack& states)
        : m_states(states)
    {}

    virtual ~GameState() = default;

    // Хуки жизненного цикла
    virtual void onEnter() {}
    virtual void onExit()  {}

    virtual void handleInput() = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;

    // === ГЛОБАЛЬНЫЙ ESC ===

    // Нужно ли вообще предлагать ConfirmExitState
    virtual bool wantsConfirmExit() const { return false; }

    // Если состояние САМО обработало ESC
    virtual bool onGlobalEscape() { return false; }
};
