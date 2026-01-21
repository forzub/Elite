// #pragma once

// #include <GLFW/glfw3.h>

// class Input
// {
// public:
//     static Input& instance();

//     // keyboard
//     void setKey(int key, bool pressed);
//     bool isKeyPressed(int key) const;
//     bool isKeyPressedOnce(int key) const;

//     // mouse
//     void setMouseButton(int button, bool pressed);
//     bool isMouseButtonPressed(int button) const;

//     void setMouseDelta(double dx, double dy);
//     double mouseDX() const;
//     double mouseDY() const;
//     void resetMouseDelta();

//     // ОБЯЗАТЕЛЬНО вызывать раз в кадр
//     void update();

// private:
//     Input() = default;

//     bool m_keys[GLFW_KEY_LAST + 1]{};
//     bool m_prevKeys[GLFW_KEY_LAST + 1]{};

//     bool m_mouseButtons[GLFW_MOUSE_BUTTON_LAST + 1]{};

//     double m_mouseDX = 0.0;
//     double m_mouseDY = 0.0;
// };


// ========================================================================
#pragma once
#include <GLFW/glfw3.h>

class Window;

class Input
{
public:
    static Input& instance();

    void update(GLFWwindow* window);

    bool isKeyPressed(int key) const;
    bool isKeyPressedOnce(int key) const;

private:
    Input() = default;

    bool m_keys[GLFW_KEY_LAST + 1]{};
    bool m_prevKeys[GLFW_KEY_LAST + 1]{};
};


