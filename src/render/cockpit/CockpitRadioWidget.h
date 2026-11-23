#pragma once

// THE MAGNITOLA WIDGET (author 2026-11-23: «сделай виджит магнитолы. кнопка лево-право, дисплей для титула станции,
// кнопка переключения между радио / свой лист, громкость. расположение - над виджетами двигательной группы»): an amber
// panel as wide as the engine group, right above it - power, the source switch (РАДИО / ЛИСТ), < display >, the signal
// and the volume -/+. Clicked with the mouse (the flight cursor is free); the keys stay (Ctrl+B ...).
#include "src/render/cockpit/EngineThrustIndicatorRenderer.h"
#include "src/render/types/Viewport.h"

namespace render::cockpit
{
class CockpitRadioWidget
{
public:
    // draws the widget anchored on the engine group's frame; a left click inside presses its buttons
    void render(const EngineThrustIndicatorRenderer::GroupFrame& frame, const Viewport& viewport);
    // true while the cursor is over the widget (the flight's own mouse use should leave the click alone)
    bool hovered() const { return m_hovered; }

private:
    struct Box
    {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        bool contains(double px, double py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
    };
    bool m_wasDown = false;
    bool m_hovered = false;
};
} // namespace render::cockpit
