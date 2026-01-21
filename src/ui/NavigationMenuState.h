#pragma once

#include "core/GameState.h"

class StateStack;

class NavigationMenuState : public GameState
{
public:
    explicit NavigationMenuState(StateStack& states);

    void handleInput() override;
    void update(float dt) override;
    void render() override;

private:

    bool m_shouldExit;
};
