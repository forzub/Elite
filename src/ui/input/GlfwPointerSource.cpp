#include "src/ui/input/GlfwPointerSource.h"

#include <GLFW/glfw3.h>

namespace ui
{

PointerSample readGlfwPointer(const Viewport& viewport, float uiW, float uiH)
{
    PointerSample s;
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window || viewport.width <= 0 || viewport.height <= 0)
        return s;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    int ww = 0, wh = 0, fbw = 0, fbh = 0;
    glfwGetWindowSize(window, &ww, &wh);
    glfwGetFramebufferSize(window, &fbw, &fbh);
    s.position = UIInputRouter::windowToUi(mx, my, ww, wh, fbw, fbh, viewport.x, viewport.y, viewport.width, viewport.height,
                                           uiW, uiH);
    s.leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    s.valid = true;
    return s;
}

} // namespace ui
