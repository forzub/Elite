#pragma once

#include <cmath>
#include <cstdint>

namespace game::client
{

/*
    Immutable-within-a-revision mapping from server simulation time to universe
    time. Packet arrival time and render frame delta never participate in this
    conversion.
*/
class ClientUniverseTimeline
{
public:
    void reset() noexcept
    {
        m_anchorServerTimeSeconds = 0.0;
        m_anchorUniverseTimeSeconds = 0.0;
        m_timeScale = 1.0;
        m_revision = 0;
        m_lastConsistencyErrorSeconds = 0.0;
        m_synchronized = false;
    }

    void synchronize(
        double serverTimeSeconds,
        double universeTimeSeconds,
        double universeTimeScale,
        std::uint64_t revision
    ) noexcept
    {
        if (!std::isfinite(serverTimeSeconds) ||
            !std::isfinite(universeTimeSeconds) ||
            !std::isfinite(universeTimeScale))
        {
            return;
        }

        if (!m_synchronized || revision != m_revision)
        {
            m_anchorServerTimeSeconds = serverTimeSeconds;
            m_anchorUniverseTimeSeconds = universeTimeSeconds;
            m_timeScale = universeTimeScale;
            m_revision = revision;
            m_lastConsistencyErrorSeconds = 0.0;
            m_synchronized = true;
            return;
        }

        const double expected = timeAtServerTime(serverTimeSeconds);
        m_lastConsistencyErrorSeconds = universeTimeSeconds - expected;

        // A scale change without a revision is a protocol violation. Keep the
        // established timeline instead of silently creating a second clock.
    }

    bool synchronized() const noexcept
    {
        return m_synchronized;
    }

    double timeAtServerTime(double serverTimeSeconds) const noexcept
    {
        if (!m_synchronized || !std::isfinite(serverTimeSeconds))
            return m_anchorUniverseTimeSeconds;

        return
            m_anchorUniverseTimeSeconds +
            (serverTimeSeconds - m_anchorServerTimeSeconds) * m_timeScale;
    }

    double timeScale() const noexcept
    {
        return m_timeScale;
    }

    std::uint64_t revision() const noexcept
    {
        return m_revision;
    }

    double lastConsistencyErrorSeconds() const noexcept
    {
        return m_lastConsistencyErrorSeconds;
    }

private:
    double m_anchorServerTimeSeconds = 0.0;
    double m_anchorUniverseTimeSeconds = 0.0;
    double m_timeScale = 1.0;
    std::uint64_t m_revision = 0;
    double m_lastConsistencyErrorSeconds = 0.0;
    bool m_synchronized = false;
};

} // namespace game::client
