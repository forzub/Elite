// The drawing of the basic widgets (UIWidgets.h): presentation only - each draws its own resolved rect.
#include "src/ui/components/UIWidgets.h"

#include <cstdlib>

#include "render/HUD/TextRenderer.h"

namespace ui
{
namespace
{
const UIWidgetStyle& styleOr(const UIWidgetStyle* s)
{
    static const UIWidgetStyle fallback;
    return s ? *s : fallback;
}

void frameRect(TextRenderer& text, const UIRect& r, float t, const glm::vec4& c)
{
    text.solidRectPx(r.x, r.y, r.width, t, c);
    text.solidRectPx(r.x, r.y + r.height - t, r.width, t, c);
    text.solidRectPx(r.x, r.y, t, r.height, c);
    text.solidRectPx(r.x + r.width - t, r.y, t, r.height, c);
}

// textDrawPx takes the BASELINE: a line centred in a box of height h sits at top + h / 2 + 0.36 px
float baselineIn(const UIRect& r, int px)
{
    return r.y + 0.5f * r.height + 0.36f * static_cast<float>(px);
}
} // namespace

void UILabel::draw(UIDrawContext& ctx)
{
    const int px = std::max(9, static_cast<int>(static_cast<float>(textPx) * ctx.scale));
    float x = m_rect.x;
    if (align == Align::Centre)
        x += 0.5f * (m_rect.width - ctx.text.measureTextPx(text, px));
    ctx.text.textDrawPx(text, x, baselineIn(m_rect, px), px, colour);
    drawChildren(ctx);
}

void UIButton::draw(UIDrawContext& ctx)
{
    const auto& s = styleOr(style);
    ctx.text.solidRectPx(m_rect.x, m_rect.y, m_rect.width, m_rect.height, lit ? s.litFill : s.fill);
    frameRect(ctx.text, m_rect, std::max(1.0f, s.borderPx * ctx.scale), lit ? s.accent : s.dim);
    const int px = std::max(9, static_cast<int>(static_cast<float>(s.textPx) * ctx.scale));
    const float tw = ctx.text.measureTextPx(label, px);
    ctx.text.textDrawPx(label, m_rect.x + 0.5f * (m_rect.width - tw), baselineIn(m_rect, px), px, lit ? s.accent : s.dim);
    drawChildren(ctx);
}

void UISlider::draw(UIDrawContext& ctx)
{
    const auto& s = styleOr(style);
    const float th = s.trackHeightPx * ctx.scale;
    const float ty = m_rect.y + 0.5f * m_rect.height - 0.5f * th;
    ctx.text.solidRectPx(m_rect.x, ty, m_rect.width, th, s.track);
    ctx.text.solidRectPx(m_rect.x, ty, m_rect.width * std::clamp(value01, 0.0f, 1.0f), th, lit ? s.accent : s.dim);
    drawChildren(ctx);
}

bool debugBoundsEnabled()
{
    static const bool on = [] { const char* v = std::getenv("ELITE_DEV_UI_BOUNDS"); return v && *v == '1'; }();
    return on;
}

void drawDebugBounds(UIDrawContext& ctx, const UIComponent& c, const UIComponent* hovered, const UIComponent* captured)
{
    if (!c.visible)
        return;
    const auto& r = c.resolvedRect();
    if (r.valid())
    {
        const glm::vec4 col = &c == captured ? glm::vec4(1.0f, 0.25f, 0.25f, 0.95f)
                            : &c == hovered  ? glm::vec4(0.3f, 1.0f, 0.4f, 0.95f)
                            : c.interactive() ? glm::vec4(0.3f, 0.7f, 1.0f, 0.75f)
                                              : glm::vec4(0.7f, 0.7f, 0.7f, 0.35f);
        frameRect(ctx.text, r, 1.0f, col);
        if (!c.id.empty())
            ctx.text.textDrawPx(c.id, r.x + 2.0f, r.y + 10.0f, 9, col);
    }
    for (const auto& child : c.childList())
        drawDebugBounds(ctx, *child, hovered, captured);
}

} // namespace ui
