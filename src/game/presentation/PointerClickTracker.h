#pragma once

#include <utility>

namespace game::presentation
{

/*
    ONE pointer interaction contract for the maps (Galaxy, System, Hub):
    - hover: continuous, never changes selection;
    - single click: acts at once (select) - it is never undone by a
      following double click, so no delayed single-click resolution exists;
    - double click: the SAME target clicked twice within the interval and
      distance below adds its action (open / close a card, focus, refine);
      a third click starts a new sequence.
    Every map uses these settings; no map keeps its own timer.
*/
struct PointerInteractionSettings
{
    double doubleClickMaxIntervalSeconds = 0.38;
    double doubleClickMaxDistancePx = 12.0;
};

inline const PointerInteractionSettings& pointerInteractionSettings()
{
    static const PointerInteractionSettings settings {};
    return settings;
}

// Double-click detector for any target key with operator== (a cube index,
// a body id, a cloud id ...).
template<typename Key>
class PointerClickTracker
{
public:
    // True when this click completes a double click on the same key.
    bool registerClick(
        double nowSeconds,
        double screenX,
        double screenY,
        const Key& key,
        const PointerInteractionSettings& settings = pointerInteractionSettings())
    {
        const double dx = screenX - m_lastX;
        const double dy = screenY - m_lastY;
        const double interval = nowSeconds - m_lastTimeSeconds;
        const bool doubleClick =
            m_valid && interval >= 0.0 &&
            interval <= settings.doubleClickMaxIntervalSeconds &&
            dx * dx + dy * dy <=
                settings.doubleClickMaxDistancePx * settings.doubleClickMaxDistancePx &&
            key == m_lastKey;
        if (doubleClick)
        {
            reset();
            return true;
        }
        m_valid = true;
        m_lastTimeSeconds = nowSeconds;
        m_lastX = screenX;
        m_lastY = screenY;
        m_lastKey = key;
        return false;
    }

    void reset()
    {
        m_valid = false;
    }

private:
    bool m_valid = false;
    double m_lastTimeSeconds = 0.0;
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    Key m_lastKey {};
};

} // namespace game::presentation
