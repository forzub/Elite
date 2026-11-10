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
    // Reactor containment siren (one 0.9 s cycle; the HUD repeats it while the alert lasts).
    void playReactorSiren() noexcept;

    // The jump core's hum (author 2026-11-09): 0 silent .. 1 the moment of the jump; it rises with
    // the charge. Sound effects are off for now - the level is kept for the future backend.
    void setJumpHum(float level01) noexcept
    {
        m_jumpHum01 = level01 < 0.0f ? 0.0f : level01 > 1.0f ? 1.0f : level01;
    }
    [[nodiscard]] float jumpHum() const noexcept
    {
        return m_jumpHum01;
    }

    [[nodiscard]] bool available() const noexcept
    {
        return m_available;
    }

private:
    AudioEngine() = default;

private:
    bool m_initialized = false;
    bool m_available = false;
    float m_jumpHum01 = 0.0f;
};

} // namespace audio
