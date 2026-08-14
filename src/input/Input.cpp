#include "Input.h"

Input& Input::instance()
{
    static Input instance;
    return instance;
}

void Input::update(GLFWwindow* window)
{
    m_scrollY = m_pendingScrollY;
    m_pendingScrollY = 0.0;

    // glfwGetKey accepts named key tokens only. Codes below
    // GLFW_KEY_SPACE (32) trigger GLFW_INVALID_ENUM.
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
    {
        m_prevKeys[key] = m_keys[key];
        m_keys[key] = glfwGetKey(window, key) == GLFW_PRESS;
    }
}



void Input::reset()
{
    for (int key = 0; key <= GLFW_KEY_LAST; ++key)
    {
        m_keys[key] = false;
        m_prevKeys[key] = false;
    }
}



bool Input::isKeyPressed(int key) const
{
    if (key < 0 || key > GLFW_KEY_LAST)
        return false;
    return m_keys[key];
}

bool Input::isKeyPressedOnce(int key) const
{
    if (key < 0 || key > GLFW_KEY_LAST)
        return false;
    return m_keys[key] && !m_prevKeys[key];
}


void Input::scrollCallback(GLFWwindow*, double, double yoffset)
{
    Input::instance().m_pendingScrollY += yoffset;
}

double Input::consumeScrollY()
{
    double v = m_scrollY;
    m_scrollY = 0.0;
    return v;
}
