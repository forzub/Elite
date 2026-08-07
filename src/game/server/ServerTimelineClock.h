#pragma once

#include <algorithm>
#include <cmath>

namespace game::server
{

/*
    Monotonic authoritative server timeline.

    This clock is deliberately independent from gameplay pause/freeze and from
    universe-time scale. It advances once per authoritative server step and is
    the time domain used by snapshots, clock synchronization and render
    interpolation.
*/
class ServerTimelineClock
{
public:
    void reset() noexcept
    {
        m_timeSeconds = 0.0;
    }

    void advance(double serverDeltaSeconds) noexcept
    {
        if (!std::isfinite(serverDeltaSeconds) || serverDeltaSeconds <= 0.0)
            return;

        m_timeSeconds += serverDeltaSeconds;
    }

    double timeSeconds() const noexcept
    {
        return m_timeSeconds;
    }

private:
    double m_timeSeconds = 0.0;
};

} // namespace game::server
