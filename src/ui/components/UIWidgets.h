#pragma once

// The minimal reusable widget set (author's brief 2026-11-24): a label, a button, a slider. Behaviour here (no GL, no
// GLFW - testable alone); their drawing in UIWidgetsDraw.cpp. Style is a plain struct (data-driven styles later).

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

#include <glm/glm.hpp>

#include "src/ui/components/UIComponent.h"

namespace ui
{

struct UIWidgetStyle
{
    glm::vec4 accent {0.96f, 0.73f, 0.18f, 0.98f};   // lit text / frame
    glm::vec4 dim {0.96f, 0.73f, 0.18f, 0.45f};      // unlit text / frame
    glm::vec4 litFill {0.96f, 0.73f, 0.18f, 0.22f};
    glm::vec4 fill {0.0f, 0.0f, 0.0f, 0.35f};
    glm::vec4 track {0.0f, 0.0f, 0.0f, 0.5f};
    float borderPx = 1.0f;     // x scale
    float trackHeightPx = 6.0f;
    int textPx = 15;           // x scale
};

class UILabel : public UIComponent
{
public:
    enum class Align { Left, Centre };
    std::string text;
    glm::vec4 colour {1.0f};
    int textPx = 15;   // x scale
    Align align = Align::Left;
    void draw(UIDrawContext& ctx) override;
};

class UIButton : public UIComponent
{
public:
    std::string label;
    bool lit = false;
    const UIWidgetStyle* style = nullptr;
    std::function<void()> onClick;
    bool interactive() const override { return true; }
    UIEventResult onPointerDown(const UIPointerEvent&) override
    {
        if (onClick)
            onClick();
        return UIEventResult::Handled;
    }
    void draw(UIDrawContext& ctx) override;
};

// its WHOLE rect is the grab area; the track is drawn inside it (thin), the value follows the pointer while captured
class UISlider : public UIComponent
{
public:
    float value01 = 0.0f;
    bool lit = false;
    const UIWidgetStyle* style = nullptr;
    std::function<void(float)> onChange;
    bool interactive() const override { return true; }
    float valueAt(const UIPoint& p) const
    {
        return m_rect.width > 0.0f ? std::clamp((p.x - m_rect.x) / m_rect.width, 0.0f, 1.0f) : value01;
    }
    UIEventResult onPointerDown(const UIPointerEvent& e) override
    {
        set(valueAt(e.position));
        return UIEventResult::HandledCapture;
    }
    UIEventResult onPointerMove(const UIPointerEvent& e) override
    {
        set(valueAt(e.position));
        return UIEventResult::Handled;
    }
    void draw(UIDrawContext& ctx) override;

private:
    void set(float v)
    {
        if (std::abs(v - value01) < 0.005f && m_touched)
            return;
        m_touched = true;
        value01 = v;
        if (onChange)
            onChange(v);
    }
    bool m_touched = false;
};

// DEV: ELITE_DEV_UI_BOUNDS=1 - every resolved rect (thin frames + ids), the hovered one and the pointer-captured one
void drawDebugBounds(UIDrawContext& ctx, const UIComponent& root, const UIComponent* hovered, const UIComponent* captured);
bool debugBoundsEnabled();

} // namespace ui
