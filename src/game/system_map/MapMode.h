#pragma once

#include <cstdint>

namespace game::system_map
{
enum class MapMode
{
    Galaxy,
    System,
    Detail,
    Hub
};

// Single owner for the persistent map submode. Interaction/presentation code
// requests a transition; render code consumes current(). Side effects belonging
// to a transition remain in SystemMapRenderer::setMode(), but the mode value
// itself is never a loose renderer flag.
class MapModeState
{
public:
    MapMode current() const noexcept
    {
        return m_current;
    }

    std::uint64_t revision() const noexcept
    {
        return m_revision;
    }

    bool transition(MapMode requested) noexcept
    {
        if (requested == m_current)
            return false;
        m_current = requested;
        ++m_revision;
        return true;
    }

    void reset(MapMode mode = MapMode::Galaxy) noexcept
    {
        if (m_current != mode)
        {
            m_current = mode;
            ++m_revision;
        }
    }

private:
    MapMode m_current = MapMode::Galaxy;
    std::uint64_t m_revision = 1;
};
}
