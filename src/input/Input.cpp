// #include "Input.h"

// Input& Input::instance()
// {
//     static Input instance;
//     return instance;
// }

// // ================= keyboard =================

// void Input::setKey(int key, bool pressed)
// {
//     if (key >= 0 && key <= GLFW_KEY_LAST)
//         m_keys[key] = pressed;
// }

// bool Input::isKeyPressed(int key) const
// {
//     if (key < 0 || key > GLFW_KEY_LAST)
//         return false;

//     return m_keys[key];
// }

// bool Input::isKeyPressedOnce(int key) const
// {
//     if (key < 0 || key > GLFW_KEY_LAST)
//         return false;

//     return m_keys[key] && !m_prevKeys[key];
// }

// // ================= mouse =================

// void Input::setMouseButton(int button, bool pressed)
// {
//     if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
//         m_mouseButtons[button] = pressed;
// }

// bool Input::isMouseButtonPressed(int button) const
// {
//     if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
//         return false;

//     return m_mouseButtons[button];
// }

// void Input::setMouseDelta(double dx, double dy)
// {
//     m_mouseDX += dx;
//     m_mouseDY += dy;
// }

// double Input::mouseDX() const { return m_mouseDX; }
// double Input::mouseDY() const { return m_mouseDY; }

// void Input::resetMouseDelta()
// {
//     m_mouseDX = 0.0;
//     m_mouseDY = 0.0;
// }

// // ================= update =================

// void Input::update()
// {
//     for (int key = 0; key <= GLFW_KEY_LAST; ++key)
//         m_prevKeys[key] = m_keys[key];
// }

// ========================================================================
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

bool Input::isKeyPressed(int key) const
{
    return m_keys[key];
}

bool Input::isKeyPressedOnce(int key) const
{
    return m_keys[key] && !m_prevKeys[key];
}



