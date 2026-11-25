#pragma once

// ONE resolved geometry of a UI component (author 2026-11-24, the UI component pass): pixels of the UI screen (the
// space TextRenderer draws in for the game viewport, origin top-left). The SAME rect is used to draw, clip, hit-test,
// lay out the children and show the debug bounds - a button and its click area are one geometric entity.

namespace ui
{

struct UIPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct UIRect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(const UIPoint& p) const
    {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }
    float right() const { return x + width; }
    float bottom() const { return y + height; }
    UIPoint centre() const { return UIPoint {x + 0.5f * width, y + 0.5f * height}; }
    bool valid() const { return width > 0.0f && height > 0.0f; }
};

} // namespace ui
