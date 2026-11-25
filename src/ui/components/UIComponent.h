#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "render/types/Viewport.h"
#include "src/ui/components/UIRect.h"

class TextRenderer;

// THE one anchor type of the UI (the second, game::UIAnchor in UIAnchor.h, was never used - removed 2026-11-25)
enum class UIAnchor
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
};

namespace ui
{
struct UIPointerEvent
{
    UIPoint position;
    int button = 0;   // 0 left
};

enum class UIEventResult
{
    Ignored,          // not mine: the router may offer it to a parent
    Handled,
    HandledCapture    // handled, and the pointer is mine until it is released (a slider drag)
};

// what a component draws with: the HUD's text / rect renderer in the UI screen's pixels, the HUD scale
struct UIDrawContext
{
    TextRenderer& text;
    float scale = 1.0f;
};
} // namespace ui

// The UI component (author's brief 2026-11-24): an isolated visual entity with its OWN resolved rect (ui::UIRect) - the
// one geometry for drawing, hit-testing and the children's layout; input reaches it only through ui::UIInputRouter as
// pointer events (it never reads GLFW); a composite instrument is built of smaller components (a button, a slider ...).
// The older render(viewport, parent...) path stays for the legacy users (the radar) until they move to layout + draw.
class UIComponent
{
public:
    virtual ~UIComponent() = default;

    std::string         id;
    bool                visible = true;
    bool                inputEnabled = true;
    glm::vec2           position {0.0f, 0.0f};   // fractions of the parent rect
    glm::vec2           size     {1.0f, 1.0f};
    float               cornerRadius = 0.0f;
    UIAnchor            anchor = UIAnchor::TopLeft;


    // ---- children (later children are drawn above and hit first) ----
    UIComponent* addChild(std::unique_ptr<UIComponent> child)
    {
        children.push_back(std::move(child));
        return children.back().get();
    }
    template <class T>
    T* add(std::unique_ptr<T> child)
    {
        T* raw = child.get();
        children.push_back(std::move(child));
        return raw;
    }
    const std::vector<std::unique_ptr<UIComponent>>& childList() const { return children; }

    // ---- the resolved geometry ----
    const ui::UIRect& resolvedRect() const { return m_rect; }
    // a parent with its own layout places a child directly
    void setResolvedRect(const ui::UIRect& rect) { m_rect = rect; }
    // the default layout: this rect from the parent's by position / size / anchor, then the children inside it
    virtual void layout(const ui::UIRect& parent)
    {
        m_rect = layoutRect(parent);
        layoutChildren();
    }
    virtual void layoutChildren()
    {
        for (auto& child : children)
            child->layout(m_rect);
    }
    ui::UIRect layoutRect(const ui::UIRect& parent) const
    {
        ui::UIRect r;
        computeLayout(parent.x, parent.y, parent.width, parent.height, r.x, r.y, r.width, r.height);
        return r;
    }

    // ---- input (only from ui::UIInputRouter) ----
    virtual bool interactive() const { return false; }
    virtual bool hitTest(const ui::UIPoint& p) const { return visible && inputEnabled && m_rect.contains(p); }
    // the deepest interactive component under p: the children front-to-back (the last drawn first), then this one
    UIComponent* deepestInteractiveAt(const ui::UIPoint& p)
    {
        if (!visible || !inputEnabled)
            return nullptr;
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (auto* hit = (*it)->deepestInteractiveAt(p))
                return hit;
        return interactive() && hitTest(p) ? this : nullptr;
    }
    virtual ui::UIEventResult onPointerDown(const ui::UIPointerEvent&) { return ui::UIEventResult::Ignored; }
    virtual ui::UIEventResult onPointerUp(const ui::UIPointerEvent&) { return ui::UIEventResult::Ignored; }
    virtual ui::UIEventResult onPointerMove(const ui::UIPointerEvent&) { return ui::UIEventResult::Ignored; }
    virtual ui::UIEventResult onScroll(const ui::UIPoint&, float) { return ui::UIEventResult::Ignored; }

    // ---- presentation: draws the already laid-out state, no input, no application calls ----
    virtual void draw(ui::UIDrawContext& ctx) { drawChildren(ctx); }
    void drawChildren(ui::UIDrawContext& ctx)
    {
        for (auto& child : children)
            if (child->visible)
                child->draw(ctx);
    }


    // ---- legacy entry point (viewport fractions) ----
    virtual void render(const Viewport& vp)
    {
        render(vp, 0.0f, 0.0f, (float)vp.width, (float)vp.height);
    }



    virtual UIComponent* findById(const std::string& searchId)
    {
        if (id == searchId)
            return this;

        for (auto& child : children)
        {
            if (auto* found = child->findById(searchId))
                return found;
        }

        return nullptr;
    }


    virtual void update(float dt)
    {
        for (auto& child : children)
            child->update(dt);
    }

protected:

    virtual void render(
        const Viewport& vp,
        float parentX,
        float parentY,
        float parentW,
        float parentH
    );

    void renderChildren(
        const Viewport& vp,
        float x,
        float y,
        float w,
        float h
    );

    protected:

    void computeLayout(
        float parentX,
        float parentY,
        float parentW,
        float parentH,
        float& outX,
        float& outY,
        float& outW,
        float& outH
    ) const;

    ui::UIRect m_rect;

private:
    std::vector<std::unique_ptr<UIComponent>>   children;

};
