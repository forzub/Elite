#pragma once

// THE UI input router (author's brief 2026-11-24, the UI component pass): the pointer is normalised to the UI screen ONCE
// at the UI boundary (windowToUi), then routed through the component tree - the deepest interactive child under it gets
// the event (front-to-back: the last drawn is hit first). A component that answers a press with HandledCapture owns the
// pointer until the release: its moves come to it even outside its rect (a slider drag) - pointer capture is the
// router's, not a flag in every widget. Widgets never read GLFW; the platform side (src/ui/input/GlfwPointerSource) does.

#include "src/ui/components/UIComponent.h"

namespace ui
{

struct PointerSample
{
    bool valid = false;
    UIPoint position;      // in the UI screen (windowToUi)
    bool leftDown = false;
    float scrollY = 0.0f;
};

class UIInputRouter
{
public:
    // routes one frame's pointer through `root`; true when the pointer is over / owned by this tree (the flight's own
    // mouse use should leave it alone)
    bool route(UIComponent& root, const PointerSample& sample);

    UIComponent* hovered() const { return m_hovered; }
    UIComponent* captured() const { return m_captured; }
    // the tree was rebuilt / hidden: drop the pointers into it
    void reset()
    {
        m_hovered = nullptr;
        m_captured = nullptr;
        m_wasDown = false;
    }

    // window cursor -> the UI screen: the window -> framebuffer scale, the letterboxed GAME viewport's offset (GL's
    // bottom-left origin -> top-left), then the viewport's pixels -> the UI screen's pixels (TextRenderer's space)
    static UIPoint windowToUi(double windowX, double windowY, int windowW, int windowH, int framebufferW, int framebufferH,
                              int viewportX, int viewportY, int viewportW, int viewportH, float uiW, float uiH);

private:
    UIComponent* m_hovered = nullptr;
    UIComponent* m_captured = nullptr;
    bool m_wasDown = false;
};

} // namespace ui
