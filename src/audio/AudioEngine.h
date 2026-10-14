#pragma once

#include <cstddef>

namespace audio
{

// Client-side audio facade.
//
// HARD BOUNDARY:
// - presentation/gameplay code submits semantic sound requests only;
// - no navigation, simulation or renderer state is visible here;
// - backend may be replaced without changing callers.
class AudioEngine final
{
public:
    static AudioEngine& instance() noexcept;

    bool initialize() noexcept;
    void shutdown() noexcept;

    // Short non-critical UI sound. Failure to play is always non-fatal.
    void playTerminalTick() noexcept;

    [[nodiscard]] bool available() const noexcept
    {
        return m_available;
    }

private:
    AudioEngine() = default;

private:
    bool m_initialized = false;
    bool m_available = false;
};

} // namespace audio
