#pragma once
#include "core/GameState.h"

class StateStack;

class ContractsState : public GameState
{
public:
    explicit ContractsState(StateStack& states);

    void handleInput() override;
    void update(float dt) override;
    void render() override;

private:

    bool m_shouldExit;
};
