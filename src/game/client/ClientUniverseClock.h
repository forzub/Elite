#pragma once

#include <algorithm>
#include <cmath>

namespace game::client
{

/*
    Client-side reconstruction of canonical universe time.

    The server periodically supplies an absolute time and a scale. Between
    authoritative synchronizations the client advances that clock from wall
    time. This clock is presentation input only: gameplay validation remains
    server-owned.
*/
class ClientUniverseClock
{
public:
    void reset() noexcept
    {
        m_timeSeconds = 0.0;
        m_timeScale = 1.0;
        m_synchronized = false;
    }

    void synchronize(
        double universeTimeSeconds,
        double universeTimeScale
    ) noexcept
    {
        if (!std::isfinite(universeTimeSeconds) ||
            !std::isfinite(universeTimeScale))
        {
            return;
        }

        m_timeSeconds = universeTimeSeconds;
        m_timeScale = universeTimeScale;
        m_synchronized = true;
    }

    void advance(double wallDeltaSeconds) noexcept
    {
        if (!m_synchronized || !std::isfinite(wallDeltaSeconds))
            return;

        const double safeDelta = std::max(0.0, wallDeltaSeconds);
        m_timeSeconds += safeDelta * m_timeScale;
    }

    bool synchronized() const noexcept
    {
        return m_synchronized;
    }

    double timeSeconds() const noexcept
    {
        return m_timeSeconds;
    }

    double timeScale() const noexcept
    {
        return m_timeScale;
    }

private:
    double m_timeSeconds = 0.0;
    double m_timeScale = 1.0;
    bool m_synchronized = false;
};

} // namespace game::client
