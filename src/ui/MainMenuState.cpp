#include "ui/MainMenuState.h"
#include "input/Input.h"
#include "core/StateStack.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

MainMenuState::MainMenuState(StateStack& states)
    : m_states(states)
    , m_shouldExit(false)
{
    const float y = 0.0f;
    const float w = 0.45f;
    const float h = 0.18f;

    m_buttons =
    {
        { "NEW GAME",  -0.6f, y, w, h },
        { "LOAD GAME",  0.0f, y, w, h },
        { "EXIT",       0.6f, y, w, h }
    };
}

void MainMenuState::handleInput()
{
    auto& input = Input::instance();

    if (input.isKeyPressed(GLFW_KEY_LEFT))
        m_selected = (m_selected + m_buttons.size() - 1) % m_buttons.size();

    if (input.isKeyPressed(GLFW_KEY_RIGHT))
        m_selected = (m_selected + 1) % m_buttons.size();

    if (input.isKeyPressed(GLFW_KEY_ENTER))
        activateSelected();

    if (input.isKeyPressed(GLFW_KEY_ESCAPE))
        m_states.clear();   // ← ВАЖНО: m_stack из State
}

void MainMenuState::update(float)
{
}

void MainMenuState::render()
{
    glDisable(GL_DEPTH_TEST);

    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        const auto& b = m_buttons[i];
        bool selected = (static_cast<int>(i) == m_selected);

        if (selected)
            glColor3f(0.9f, 0.8f, 0.2f);
        else
            glColor3f(0.4f, 0.4f, 0.45f);

        float x0 = b.cx - b.w * 0.5f;
        float x1 = b.cx + b.w * 0.5f;
        float y0 = b.cy - b.h * 0.5f;
        float y1 = b.cy + b.h * 0.5f;

        glBegin(GL_QUADS);
            glVertex2f(x0, y0);
            glVertex2f(x1, y0);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
        glEnd();
    }
}

void MainMenuState::activateSelected()
{
    switch (m_selected)
    {
        case 0: // NEW GAME
            m_states.pop();
            // позже: m_states.push(SpaceState)
            break;

        case 1: // LOAD GAME
            // заглушка
            break;

        case 2: // EXIT
            m_states.clear();
            break;
    }
}
