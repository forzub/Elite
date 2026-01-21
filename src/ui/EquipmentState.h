#pragma once
#include "core/GameState.h"

class StateStack;

class EquipmentState : public GameState
{
public:
    explicit EquipmentState(StateStack& states);

    void handleInput() override;
    void update(float dt) override;
    void render() override;

private:

    bool m_shouldExit;
};
