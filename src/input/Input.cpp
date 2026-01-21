#include "Input.h"

Input& Input::instance()
{
    static Input instance;
    return instance;
}

void Input::setKey(int key, bool pressed)
{
    m_keys[key] = pressed;
}

bool Input::isKeyPressed(int key) const
{
    auto it = m_keys.find(key);
    if (it == m_keys.end())
        return false;

    return it->second;
}

void Input::setMouseDelta(double dx, double dy)
{
    m_mouseDX += dx;
    m_mouseDY += dy;
}

double Input::mouseDX() const
{
    return m_mouseDX;
}

double Input::mouseDY() const
{
    return m_mouseDY;
}

void Input::resetMouseDelta()
{
    m_mouseDX = 0.0;
    m_mouseDY = 0.0;
}

void Input::setMouseButton(int button, bool pressed)
{
    m_mouseButtons[button] = pressed;
}

bool Input::isMouseButtonPressed(int button) const
{
    auto it = m_mouseButtons.find(button);
    if (it == m_mouseButtons.end())
        return false;
    return it->second;
}
