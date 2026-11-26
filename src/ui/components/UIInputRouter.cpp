#include "src/ui/components/UIInputRouter.h"

#include <vector>

namespace ui
{

namespace
{
// the chain root .. target (target last); false: target is not in this tree
bool chainTo(UIComponent& from, const UIComponent* target, std::vector<UIComponent*>& chain)
{
    chain.push_back(&from);
    if (&from == target)
        return true;
    for (const auto& child : from.childList())
        if (child && chainTo(*child, target, chain))
            return true;
    chain.pop_back();
    return false;
}

// BUBBLING (audit 2026-11-26 LOW: UIEventResult::Ignored promised "the router may offer it to a parent" and the router
// never did): the event goes to the target, then up its ancestors until one does not ignore it. Returns the one that
// took it (nullptr: nobody) and its answer.
template <class Call>
UIComponent* bubble(UIComponent& root, UIComponent* target, Call&& call, UIEventResult& result)
{
    result = UIEventResult::Ignored;
    if (!target)
        return nullptr;
    std::vector<UIComponent*> chain;
    if (!chainTo(root, target, chain))
        chain.assign(1, target);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        result = call(**it);
        if (result != UIEventResult::Ignored)
            return *it;
    }
    return nullptr;
}
} // namespace

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
    UIEventResult result = UIEventResult::Ignored;
    if (pressed && m_hovered)
    {
        // the one that took the press (the target or an ancestor) owns the pointer when it asks to
        UIComponent* taker = bubble(root, m_hovered, [&](UIComponent& c) { return c.onPointerDown(ev); }, result);
        if (result == UIEventResult::HandledCapture)
            m_captured = taker;
    }
    else if (sample.leftDown && m_captured)
        (void)m_captured->onPointerMove(ev);   // a capture is the captor's alone: no bubbling
    else if (released)
    {
        if (m_captured)
            (void)m_captured->onPointerUp(ev);
        else
            (void)bubble(root, m_hovered, [&](UIComponent& c) { return c.onPointerUp(ev); }, result);
        m_captured = nullptr;
    }
    if (sample.scrollY != 0.0f && m_hovered)
        (void)bubble(root, m_hovered, [&](UIComponent& c) { return c.onScroll(sample.position, sample.scrollY); },
                     result);
    m_wasDown = sample.leftDown;
    const auto& r = root.resolvedRect();
    return m_captured != nullptr || m_hovered != nullptr || (root.visible && r.contains(sample.position));
}

} // namespace ui
