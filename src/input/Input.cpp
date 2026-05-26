#include "Input.h"

Input& Input::instance()
{
    static Input instance;
    return instance;
}

void Input::update(GLFWwindow* window)
{
    for (int key = 0; key <= GLFW_KEY_LAST; ++key)
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
    return m_keys[key];
}

bool Input::isKeyPressedOnce(int key) const
{
    return m_keys[key] && !m_prevKeys[key];
}



