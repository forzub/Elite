#include "src/ui/components/UIInputRouter.h"

namespace ui
{

UIPoint UIInputRouter::windowToUi(double windowX, double windowY, int windowW, int windowH, int framebufferW,
                                  int framebufferH, int viewportX, int viewportY, int viewportW, int viewportH, float uiW,
                                  float uiH)
{
    double x = windowX, y = windowY;
    if (windowW > 0 && windowH > 0 && framebufferW > 0 && framebufferH > 0)
    {
        x = x * static_cast<double>(framebufferW) / windowW;
        y = y * static_cast<double>(framebufferH) / windowH;
    }
    x -= static_cast<double>(viewportX);
    y -= static_cast<double>(framebufferH - viewportY - viewportH);   // GL's bottom offset -> the top offset
    if (viewportW > 0 && viewportH > 0)
    {
        x = x * (uiW > 1.0f ? uiW : viewportW) / viewportW;
        y = y * (uiH > 1.0f ? uiH : viewportH) / viewportH;
    }
    return UIPoint {static_cast<float>(x), static_cast<float>(y)};
}

bool UIInputRouter::route(UIComponent& root, const PointerSample& sample)
{
    if (!sample.valid)
        return false;
    UIPointerEvent ev;
    ev.position = sample.position;
    m_hovered = m_captured ? m_captured : root.deepestInteractiveAt(sample.position);
    const bool pressed = sample.leftDown && !m_wasDown;
    const bool released = !sample.leftDown && m_wasDown;
    if (pressed && m_hovered)
    {
        if (m_hovered->onPointerDown(ev) == UIEventResult::HandledCapture)
            m_captured = m_hovered;
    }
    else if (sample.leftDown && m_captured)
        (void)m_captured->onPointerMove(ev);
    else if (released)
    {
        if (m_captured)
            (void)m_captured->onPointerUp(ev);
        else if (m_hovered)
            (void)m_hovered->onPointerUp(ev);
        m_captured = nullptr;
    }
    if (sample.scrollY != 0.0f && m_hovered)
        (void)m_hovered->onScroll(sample.position, sample.scrollY);
    m_wasDown = sample.leftDown;
    const auto& r = root.resolvedRect();
    return m_captured != nullptr || m_hovered != nullptr || (root.visible && r.contains(sample.position));
}

} // namespace ui
