#pragma once

#include "core/GameState.h"
#include <vector>
#include <string>

class StateStack;

class MainMenuState : public GameState
{
public:
    explicit MainMenuState(StateStack& states);

    void handleInput() override;
    void update(float dt) override;
    void render() override;

private:
    struct Button
    {
        std::string label;
        float cx;
        float cy;
        float w;
        float h;
    };

    std::vector<Button> m_buttons;
    int m_selected = 0;

private:
    void activateSelected();
    StateStack& m_states;
    bool m_shouldExit;

};
