#include "ui/MainMenuState.h"

#include <glad/gl.h>

MainMenuState::MainMenuState(StateStack& states)
    : GameState(states)
{
}

void MainMenuState::handleInput()
{
    // The visible service shell is a GameWebView child window. Keyboard/focus
    // handling belongs to that shell so there is one authoritative menu input
    // path instead of a second hidden GLFW menu implementation.
}

void MainMenuState::update(float)
{
}

void MainMenuState::render()
{
}

void MainMenuState::renderUI()
{
    glClearColor(0.007f, 0.010f, 0.020f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
