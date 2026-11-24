#include "src/render/cockpit/CockpitRadioWidget.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <GLFW/glfw3.h>

#include "src/audio/CockpitRadio.h"
#include "src/render/HUD/TextRenderer.h"
#include "src/render/cockpit/EngineStatusWidgetStyle.h"

namespace render::cockpit
{
namespace
{
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
            cut.pop_back();   // the lead byte of the code point
        if (text.measureTextPx(cut + "...", px) <= maxPx)
            return cut + "...";
    }
    return "...";
}

void frameRect(TextRenderer& text, float x, float y, float w, float h, float t, const glm::vec4& c)
{
    text.solidRectPx(x, y, w, t, c);
    text.solidRectPx(x, y + h - t, w, t, c);
    text.solidRectPx(x, y, t, h, c);
    text.solidRectPx(x + w - t, y, t, h, c);
}
} // namespace

void CockpitRadioWidget::render(const EngineThrustIndicatorRenderer::GroupFrame& frame, const Viewport& viewport)
{
    if (!frame.valid || viewport.width <= 0 || viewport.height <= 0)
        return;
    auto& radio = audio::CockpitRadio::instance();
    const auto d = radio.display();
    auto& text = TextRenderer::instance();
    const auto& style = resolveEngineStatusWidgetStyle("human_amber");
    const float s = frame.scale;
    const float gap = 9.0f * s;
    const float w = std::max(260.0f * s, frame.right - frame.left);
    const float rowH = 30.0f * s;
    const float h = 3.0f * rowH + 16.0f * s;
    const float x = frame.left;
    const float y = frame.top - gap - h;
    const glm::vec4 amber = style.label;
    const glm::vec4 dim(amber.r, amber.g, amber.b, 0.45f);
    const glm::vec4 lcd(0.35f, 0.95f, 0.80f, 1.0f);
    const int small = std::max(9, static_cast<int>(15.0f * s));
    const int big = static_cast<int>(19.0f * s);

    // the panel
    text.solidRectPx(x, y, w, h, style.panelBackground);
    frameRect(text, x, y, w, h, std::max(1.0f, 1.0f * s), style.panelBorder);

    const float pad = 8.0f * s;
    const float btn = rowH - 6.0f * s;
    // row 1: МАГНИТОЛА · the source switch · the power
    const float r1 = y + pad;
    // textDrawPx takes the BASELINE: a line centred in a row of height hh sits at top + hh / 2 + 0.36 px
    const auto mid = [](float top, float hh, int px) { return top + 0.5f * hh + 0.36f * static_cast<float>(px); };
    text.textDrawPx("МАГНИТОЛА", x + pad, mid(r1, rowH - 6.0f * s, small), small, amber);
    const Box power {x + w - pad - 58.0f * s, r1, 58.0f * s, btn};
    const Box sourceBox {power.x - gap - 96.0f * s, r1, 96.0f * s, btn};
    const auto button = [&](const Box& b, const std::string& label, bool lit) {
        text.solidRectPx(b.x, b.y, b.w, b.h, lit ? glm::vec4(amber.r, amber.g, amber.b, 0.22f) : glm::vec4(0.0f, 0.0f, 0.0f, 0.35f));
        frameRect(text, b.x, b.y, b.w, b.h, std::max(1.0f, 1.0f * s), lit ? amber : dim);
        const float tw = text.measureTextPx(label, small);
        text.textDrawPx(label, b.x + 0.5f * (b.w - tw), mid(b.y, b.h, small), small, lit ? amber : dim);
    };
    button(power, d.on ? "ВКЛ" : "ВЫКЛ", d.on);
    button(sourceBox, d.source == audio::CockpitRadio::Source::Radio ? "РАДИО" : "ЛИСТ", d.on);

    // row 2: < the display >
    const float r2 = r1 + rowH;
    const Box prev {x + pad, r2, btn, btn};
    const Box next {x + w - pad - btn, r2, btn, btn};
    button(prev, "<", d.on);
    button(next, ">", d.on);
    const Box lcdBox {prev.x + prev.w + 6.0f * s, r2, next.x - 6.0f * s - (prev.x + prev.w + 6.0f * s), btn};
    text.solidRectPx(lcdBox.x, lcdBox.y, lcdBox.w, lcdBox.h, glm::vec4(0.01f, 0.06f, 0.05f, 0.95f));
    frameRect(text, lcdBox.x, lcdBox.y, lcdBox.w, lcdBox.h, std::max(1.0f, 1.0f * s), glm::vec4(lcd.r, lcd.g, lcd.b, 0.35f));
    const std::string title = d.on ? d.title + (d.connecting ? " ..." : "") : "---";
    text.textDrawPx(fitText(text, title, big, lcdBox.w - 12.0f * s), lcdBox.x + 6.0f * s,
                    mid(lcdBox.y, lcdBox.h, big), big, d.on ? lcd : dim);

    // row 3: the zone / counter + the signal bar, the volume - bar +
    const float r3 = r2 + rowH;
    const Box volDown {x + w - pad - 2.0f * btn - 70.0f * s - 8.0f * s, r3, btn, btn};
    const Box volUp {x + w - pad - btn, r3, btn, btn};
    button(volDown, "-", d.on);
    button(volUp, "+", d.on);
    const float vbx = volDown.x + btn + 4.0f * s, vbw = volUp.x - 4.0f * s - vbx;
    text.solidRectPx(vbx, r3 + 0.5f * btn - 3.0f * s, vbw, 6.0f * s, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    text.solidRectPx(vbx, r3 + 0.5f * btn - 3.0f * s, vbw * static_cast<float>(d.volume) / 100.0f, 6.0f * s, d.on ? amber : dim);
    float infoRight = volDown.x - gap;
    if (d.on && d.signal >= 0)
    {
        // the signal: 5 bars, the noise is the rest
        const int bars = (d.signal + 10) / 20;
        const float bw = 6.0f * s, bg = 3.0f * s;
        const float bx = infoRight - 5.0f * (bw + bg);
        for (int i = 0; i < 5; ++i)
        {
            const float bh = (6.0f + 4.0f * static_cast<float>(i)) * s;
            text.solidRectPx(bx + static_cast<float>(i) * (bw + bg), r3 + btn - bh - 2.0f * s, bw, bh,
                             i < bars ? lcd : glm::vec4(lcd.r, lcd.g, lcd.b, 0.18f));
        }
        infoRight = bx - gap;
    }
    // the playlist: "x" takes the current track out of the container
    const bool playlistMode = d.on && d.source == audio::CockpitRadio::Source::Playlist && radio.playlistSize() > 0;
    const Box removeBox {x + pad, r3, btn, btn};
    float infoLeft = x + pad;
    if (playlistMode)
    {
        button(removeBox, "x", true);
        infoLeft = removeBox.x + removeBox.w + 6.0f * s;
    }
    if (d.on)
        text.textDrawPx(fitText(text, d.header, small, infoRight - infoLeft), infoLeft, mid(r3, btn, small), small, amber);

    // the mouse: a left press inside a button
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    int ww = 0, wh = 0, fbw = 0, fbh = 0;
    glfwGetWindowSize(window, &ww, &wh);
    glfwGetFramebufferSize(window, &fbw, &fbh);
    // window -> framebuffer pixels -> the GAME VIEWPORT the widget is drawn in. The game renders letterboxed (16:9 inside
    // the window, Application: glViewport(lb.x, lb.y, ...)): on a non-16:9 monitor the viewport is OFFSET and every click
    // missed its button (author 2026-11-24: «клики по кнопкам радио все еще не попадают») - the offset was never removed.
    // The same mapping the system map uses (SystemMapRendererInput: mx - vp.x).
    if (ww > 0 && wh > 0 && fbw > 0 && fbh > 0)
    {
        mx = mx * static_cast<double>(fbw) / ww;
        my = my * static_cast<double>(fbh) / wh;
    }
    mx -= static_cast<double>(viewport.x);
    my -= static_cast<double>(fbh - viewport.y - viewport.height);   // GL's bottom offset -> the top offset
    // the viewport's pixels -> the pixel space the text is drawn in (equal unless the HUD is drawn at another size)
    const double sw = text.screenWidthPx() > 1.0f ? text.screenWidthPx() : static_cast<double>(viewport.width);
    const double sh = text.screenHeightPx() > 1.0f ? text.screenHeightPx() : static_cast<double>(viewport.height);
    if (viewport.width > 0 && viewport.height > 0)
    {
        mx = mx * sw / viewport.width;
        my = my * sh / viewport.height;
    }
    const Box panel {x, y, w, h};
    const Box volBar {vbx, r3, vbw, btn};
    m_hovered = panel.contains(mx, my) || m_draggingVolume;
    const bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const auto barVolume = [&]() { return static_cast<float>((mx - vbx) / std::max(1.0f, vbw)); };
    if (down && !m_wasDown && panel.contains(mx, my))
    {
        const char* hit = "panel";
        if (power.contains(mx, my))
            radio.togglePower(), hit = "power";
        else if (sourceBox.contains(mx, my))
            radio.switchSource(), hit = "source";
        else if (prev.contains(mx, my))
            radio.previous(), hit = "prev";
        else if (next.contains(mx, my))
            radio.next(), hit = "next";
        else if (volDown.contains(mx, my))
            radio.volumeStep(-1), hit = "vol-";
        else if (volUp.contains(mx, my))
            radio.volumeStep(1), hit = "vol+";
        else if (volBar.contains(mx, my))
        {
            radio.setVolume01(barVolume());
            m_draggingVolume = true;
            hit = "volume bar";
        }
        else if (playlistMode && removeBox.contains(mx, my))
            radio.removeCurrentTrack(), hit = "remove";
        std::fprintf(stderr, "[RadioUI] press at (%.0f, %.0f) of %.0fx%.0f (window %dx%d), panel (%.0f, %.0f %.0fx%.0f) -> %s\n",
                     mx, my, sw, sh, ww, wh, x, y, w, h, hit);
    }
    else if (down && m_draggingVolume)
    {
        const int now = static_cast<int>(std::clamp(barVolume(), 0.0f, 1.0f) * 100.0f + 0.5f);
        if (now != d.volume)
            radio.setVolume01(barVolume());
    }
    if (!down)
        m_draggingVolume = false;
    m_wasDown = down;
}
} // namespace render::cockpit
