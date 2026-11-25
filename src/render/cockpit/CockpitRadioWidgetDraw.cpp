// The magnitola's drawing (CockpitRadioWidget.h): presentation only - the panel's own background and frame, then the
// children, each in its own resolved rect.
#include "src/render/cockpit/CockpitRadioWidget.h"
#include "src/render/cockpit/CockpitRadioWidgetParts.h"

#include <algorithm>

#include "src/render/HUD/TextRenderer.h"
#include "src/render/cockpit/EngineStatusWidgetStyle.h"

namespace render::cockpit
{
namespace
{
void frameRect(TextRenderer& text, const ui::UIRect& r, float t, const glm::vec4& c)
{
    text.solidRectPx(r.x, r.y, r.width, t, c);
    text.solidRectPx(r.x, r.y + r.height - t, r.width, t, c);
    text.solidRectPx(r.x, r.y, t, r.height, c);
    text.solidRectPx(r.x + r.width - t, r.y, t, r.height, c);
}

// a UTF-8 string cut to fit maxPx (whole code points), "..." when cut
std::string fitText(TextRenderer& text, const std::string& s, int px, float maxPx)
{
    if (text.measureTextPx(s, px) <= maxPx)
        return s;
    std::string cut = s;
    while (!cut.empty())
    {
        do
            cut.pop_back();
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0u) == 0x80u);
        if (!cut.empty())
            cut.pop_back();
        if (text.measureTextPx(cut + "...", px) <= maxPx)
            return cut + "...";
    }
    return "...";
}
} // namespace

void CockpitRadioWidget::draw(ui::UIDrawContext& ctx)
{
    if (!m_rect.valid())
        return;
    const auto& amber = resolveEngineStatusWidgetStyle("human_amber");
    m_buttonStyle.accent = amber.label;
    m_buttonStyle.dim = glm::vec4(amber.label.r, amber.label.g, amber.label.b, 0.45f);
    m_buttonStyle.litFill = glm::vec4(amber.label.r, amber.label.g, amber.label.b, 0.22f);
    m_caption->colour = amber.label;
    m_header->colour = amber.label;
    ctx.text.solidRectPx(m_rect.x, m_rect.y, m_rect.width, m_rect.height, amber.panelBackground);
    frameRect(ctx.text, m_rect, std::max(1.0f, ctx.scale), amber.panelBorder);
    // the header may be longer than its room: cut to fit (the label draws what it is given)
    if (m_header->visible)
        m_header->text = fitText(ctx.text, m_presentation.header,
                                 std::max(9, static_cast<int>(m_style.smallText * ctx.scale)), m_header->resolvedRect().width);
    drawChildren(ctx);
}

void RadioStationDisplay::draw(ui::UIDrawContext& ctx)
{
    const RadioWidgetStyle fallback;
    const auto& st = style ? *style : fallback;
    const auto& dimSrc = resolveEngineStatusWidgetStyle("human_amber").label;
    ctx.text.solidRectPx(m_rect.x, m_rect.y, m_rect.width, m_rect.height, st.lcdFill);
    frameRect(ctx.text, m_rect, std::max(1.0f, ctx.scale), glm::vec4(st.lcd.r, st.lcd.g, st.lcd.b, 0.35f));
    const int px = static_cast<int>(st.bigText * ctx.scale);
    const float baseline = m_rect.y + 0.5f * m_rect.height + 0.36f * static_cast<float>(px);
    ctx.text.textDrawPx(fitText(ctx.text, text, px, m_rect.width - 12.0f * ctx.scale), m_rect.x + 6.0f * ctx.scale, baseline, px,
                        lit ? st.lcd : glm::vec4(dimSrc.r, dimSrc.g, dimSrc.b, 0.45f));
}

void RadioSignalIndicator::draw(ui::UIDrawContext& ctx)
{
    const RadioWidgetStyle fallback;
    const auto& st = style ? *style : fallback;
    // 5 bars, the noise is the rest
    const int bars = (signal + 10) / 20;
    const float s = ctx.scale;
    const float bw = 6.0f * s, bg = 3.0f * s;
    for (int i = 0; i < 5; ++i)
    {
        const float bh = (6.0f + 4.0f * static_cast<float>(i)) * s;
        ctx.text.solidRectPx(m_rect.x + static_cast<float>(i) * (bw + bg), m_rect.bottom() - bh - 2.0f * s, bw, bh,
                             i < bars ? st.lcd : glm::vec4(st.lcd.r, st.lcd.g, st.lcd.b, 0.18f));
    }
}

} // namespace render::cockpit
