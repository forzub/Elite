#pragma once

#include "core/StateStack.h"
#include "core/StateContext.h"


// class StateStack;
// struct StateContext;

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

    // Called once per application frame before input. States may resolve an
    // immutable presentation/input snapshot here. The default is a no-op.
    virtual void prepareFrame(float dt) { (void)dt; }

    virtual void handleInput() = 0;
    virtual void update(float dt) = 0;

    virtual void renderUI() {}
    virtual void renderHUD() {}
    virtual void render() = 0;

    virtual bool isModal() const { return false; }
    virtual void handleResize(int width, int height) {}

    // Player-facing UI language changed. Cockpit/manufacturer-language layers
    // may intentionally ignore this global locale.
    virtual void onUiLanguageChanged() {}


    // === ГЛОБАЛЬНЫЙ ESC ===

    // Нужно ли вообще предлагать ConfirmExitState
    virtual bool wantsConfirmExit() const { return false; }

    // Если состояние САМО обработало ESC
    virtual bool onGlobalEscape() { return false; }

protected:
    StateContext& context()
    {
        return m_states.context();
    }


};
