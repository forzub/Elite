#pragma once

#include "core/GameState.h"

class StateStack;

// Lightweight native state that keeps the render loop alive behind the
// GameWebView service shell. Main-menu interaction/navigation is owned by
// Application + main_menu.html, not duplicated through HtmlUiManager.
class MainMenuState : public GameState
{
public:
    explicit MainMenuState(StateStack& states);

    void handleInput() override;
    void update(float dt) override;
    void renderUI() override;
    void render() override;
};
