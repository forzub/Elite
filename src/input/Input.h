
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

    void reset();

private:
    Input() = default;

    bool m_keys[GLFW_KEY_LAST + 1]{};
    bool m_prevKeys[GLFW_KEY_LAST + 1]{};
};


