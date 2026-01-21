#pragma once

#include "core/GameState.h"

class StateStack;

class TradeMenuState : public GameState
{
public:
    explicit TradeMenuState(StateStack& states);

    void handleInput() override;
    void update(float dt) override;
    void render() override;

private:

    bool m_shouldExit;
};
