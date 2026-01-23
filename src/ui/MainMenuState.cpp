#include "ui/MainMenuState.h"
#include "game/SpaceState.h"
#include "core/StateStack.h"

#include <cassert>

#include <glad/gl.h>
#include <GLFW/glfw3.h>


namespace
{
    int toInt(MenuItem item)
    {
        return static_cast<int>(item);
    }

    MenuItem fromInt(int value)
    {
        return static_cast<MenuItem>(value);
    }
}


// =====================================================================================
// Constructor
// =====================================================================================

MainMenuState::MainMenuState(StateStack& states)
    : GameState(states)
    , m_shouldExit(false)
    , m_selected(MenuItem::NewGame)
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

    assert(m_buttons.size() == static_cast<size_t>(MenuItem::Count));
}


// =====================================================================================
// Input
// =====================================================================================

void MainMenuState::handleInput()
{
    GLFWwindow* window = glfwGetCurrentContext();

    bool left  = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool enter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    bool esc   = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

    // ←
    if (left && !m_leftPressed)
        selectPrevious();

    // →
    if (right && !m_rightPressed)
        selectNext();

    // Enter
    if (enter && !m_enterPressed)
    {
        activateSelected();
    }

    // Esc
    if (esc && !m_escPressed)
    {
        m_states.clear();
    }

    // запоминаем текущее состояние
    m_leftPressed  = left;
    m_rightPressed = right;
    m_enterPressed = enter;
    m_escPressed   = esc;
}



// =====================================================================================
// Update
// =====================================================================================

void MainMenuState::update(float)
{
}



// =====================================================================================
// Render
// =====================================================================================

void MainMenuState::render()
{
    glDisable(GL_DEPTH_TEST);

    // glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        const auto& b = m_buttons[i];
        bool selected = isSelected(i);

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


// =====================================================================================
// activate Selected
// =====================================================================================

void MainMenuState::activateSelected()
{
    switch (m_selected)
    {
        case MenuItem::NewGame:
            m_states.pop();
            m_states.push(std::make_unique<SpaceState>(m_states));
            break;

        case MenuItem::LoadGame:
            // заглушка
            break;

        case MenuItem::Exit:
            m_states.clear();
            break;

        case MenuItem::Count:
            break; // технический случай, сюда не должны попадать
    }
}


// =====================================================================================
// →
// =====================================================================================

void MainMenuState::selectNext()
{
    int index = toInt(m_selected);
    int count = toInt(MenuItem::Count);

    index = (index + 1) % count;
    m_selected = fromInt(index);
}


// =====================================================================================
// ←
// =====================================================================================

void MainMenuState::selectPrevious()
{
    int index = toInt(m_selected);
    int count = toInt(MenuItem::Count);

    index = (index - 1 + count) % count;
    m_selected = fromInt(index);
}

// =====================================================================================
// Реализация isSelected
// =====================================================================================

bool MainMenuState::isSelected(size_t index) const
{
    return static_cast<MenuItem>(index) == m_selected;
}