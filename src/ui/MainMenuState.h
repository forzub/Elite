#pragma once

#include "core/GameState.h"
#include <vector>
#include <string>

class StateStack;


enum class MenuItem {
    NewGame = 0,
    LoadGame,
    Exit,
    Count
};


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


private:
    void activateSelected();
    
    bool m_shouldExit;

    MenuItem m_selected;
    void selectNext();
    void selectPrevious();

    bool m_leftPressed  = false;
    bool m_rightPressed = false;
    bool m_enterPressed = false;
    bool m_escPressed   = false;

    bool isSelected(size_t index) const;

};
