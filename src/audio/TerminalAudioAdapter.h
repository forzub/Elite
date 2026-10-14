#pragma once

#include <cstddef>

namespace audio
{

// Pure rate limiter between typewriter presentation and audio playback.
// It owns no AudioEngine pointer and performs no I/O.
class TerminalAudioAdapter final
{
public:
    [[nodiscard]] bool shouldPlayTick(
        std::size_t typedCharacters,
        double nowSeconds
    ) noexcept
    {
        if (typedCharacters == 0)
            return false;

        constexpr double MinimumIntervalSeconds = 0.045;

        if (!m_hasPlayed ||
            nowSeconds - m_lastTickSeconds >= MinimumIntervalSeconds)
        {
            m_hasPlayed = true;
            m_lastTickSeconds = nowSeconds;
            return true;
        }

        return false;
    }

    void reset() noexcept
    {
        m_hasPlayed = false;
        m_lastTickSeconds = 0.0;
    }

private:
    bool m_hasPlayed = false;
    double m_lastTickSeconds = 0.0;
};

} // namespace audio
