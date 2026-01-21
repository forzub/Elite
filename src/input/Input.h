#pragma once

#include <unordered_map>

class Input
{
public:
    static Input& instance();

    void setKey(int key, bool pressed);

    void setMouseDelta(double dx, double dy);
    double mouseDX() const;
    double mouseDY() const;
    void resetMouseDelta();

    bool isKeyPressed(int key) const;

    void setMouseButton(int button, bool pressed);
    bool isMouseButtonPressed(int button) const;

private:
    Input() = default;

    std::unordered_map<int, bool> m_keys;
    std::unordered_map<int, bool> m_mouseButtons;
    double m_mouseDX = 0.0;
    double m_mouseDY = 0.0;
};
