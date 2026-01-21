#include "ui/ConfirmExitState.h"

#include "core/StateStack.h"
#include "ui/MainMenuState.h"
#include "input/Input.h"

#include <GLFW/glfw3.h>
#include <iostream>

// =====================================================================================
// Constructor
// =====================================================================================

ConfirmExitState::ConfirmExitState(StateStack& states, const ConfirmExitOptions& options)
    : GameState(states)
    , m_options(options)
{

    
    // ВСЕ кнопки рисуются всегда
    m_actions.push_back(ExitAction::Continue);
    m_actions.push_back(ExitAction::Save);
    m_actions.push_back(ExitAction::Load);
    m_actions.push_back(ExitAction::ExitToMenu);
}

// =====================================================================================
// deConstructor
// =====================================================================================

ConfirmExitState::~ConfirmExitState()
{
    
}

// =====================================================================================
// Input
// =====================================================================================

void ConfirmExitState::handleInput()
{
    

    GLFWwindow* window = glfwGetCurrentContext();

    bool left  = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool enter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    // bool esc   = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    // bool esc = Input::instance().isKeyPressed(GLFW_KEY_ESCAPE);

    if (left && !m_leftPressed)
        selectPrevious();

    if (right && !m_rightPressed)
        selectNext();

    if (enter && !m_enterPressed)
        activateSelected();

    // ESC = то же, что "Продолжить"
    // if (esc && !m_escPressed)
    //     m_states.pop();

    m_leftPressed  = left;
    m_rightPressed = right;
    m_enterPressed = enter;
    // m_escPressed   = esc;
}

// =====================================================================================
// Update (пока пустые)
// =====================================================================================

void ConfirmExitState::update(float)
{
}


// =====================================================================================
// Render (пока пустые)
// =====================================================================================

void ConfirmExitState::render()
{
    
    // Пока пусто.
    // Здесь позже будет:
    // - затемнение фона
    // - рамка
    // - текст

                glDisable(GL_DEPTH_TEST);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();

                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                glColor3f(1.0f, 0.0f, 0.0f); // КРАСНЫЙ

                glBegin(GL_QUADS);
                    glVertex2f(-0.6f, -0.2f);
                    glVertex2f( 0.6f, -0.2f);
                    glVertex2f( 0.6f,  0.2f);
                    glVertex2f(-0.6f,  0.2f);
                glEnd();
}

// =====================================================================================
// Logic
// =====================================================================================

void ConfirmExitState::activateSelected()
{
    ExitAction action = m_actions[m_selectedIndex];

    if (!isActionEnabled(action))
        return;

    switch (action)
    {
        case ExitAction::Continue:
            m_states.pop();
            break;

        case ExitAction::Save:
            // TODO: SaveManager::save();
            // После сохранения — отключаем кнопку
            m_options.canSave = false;
            break;

        case ExitAction::Load:
            // TODO: SaveManager::load();
            m_states.pop(); // закрываем ConfirmExitState
            break;

        case ExitAction::ExitToMenu:
            m_states.pop(); // ConfirmExitState
            m_states.pop(); // SpaceState
            m_states.push(std::make_unique<MainMenuState>(m_states));
            break;

        default:
            break;
    }
}

// =====================================================================================
// Helpers
// =====================================================================================

bool ConfirmExitState::isActionEnabled(ExitAction action) const
{
    switch (action)
    {
        case ExitAction::Save:
            return m_options.canSave;

        case ExitAction::Load:
            return m_options.canLoad;

        default:
            return true;
    }
}


// =====================================================================================
// selectNext
// =====================================================================================

void ConfirmExitState::selectNext()
{
    m_selectedIndex = (m_selectedIndex + 1) % m_actions.size();
}


// =====================================================================================
// selectPrevious
// =====================================================================================

void ConfirmExitState::selectPrevious()
{
    m_selectedIndex =
        (m_selectedIndex + m_actions.size() - 1) % m_actions.size();
}


// =====================================================================================
// onGlobalEscape
// =====================================================================================


bool ConfirmExitState::onGlobalEscape()
{
    m_states.pop(); // "Продолжить"
    return true;
}


// bool ConfirmExitState::wantsConfirmExit() const
// {
//     return false;
// }


