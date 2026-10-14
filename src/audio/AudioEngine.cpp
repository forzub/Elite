#include "src/audio/AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace audio
{
namespace
{
#ifdef _WIN32
std::vector<unsigned char> makeTerminalTickWav()
{
    constexpr int SampleRate = 22050;
    constexpr double DurationSeconds = 0.045;
    constexpr int SampleCount =
        static_cast<int>(SampleRate * DurationSeconds);

    std::vector<std::int16_t> pcm(
        static_cast<std::size_t>(SampleCount)
    );

    // Deliberately dull, short electromechanical "puk":
    // low fundamental + weak harmonic under a steep exponential envelope.
    constexpr double Pi = 3.14159265358979323846;
    for (int i = 0; i < SampleCount; ++i)
    {
        const double t =
            static_cast<double>(i) /
            static_cast<double>(SampleRate);
        const double envelope =
            std::exp(-t * 72.0);
        const double fundamental =
            std::sin(2.0 * Pi * 185.0 * t);
        const double harmonic =
            0.22 * std::sin(2.0 * Pi * 370.0 * t + 0.35);
        const double sample =
            std::clamp(
                (fundamental + harmonic) * envelope * 0.22,
                -1.0,
                1.0
            );

        pcm[static_cast<std::size_t>(i)] =
            static_cast<std::int16_t>(
                sample * 32767.0
            );
    }

    constexpr std::uint16_t Channels = 1;
    constexpr std::uint16_t BitsPerSample = 16;
    constexpr std::uint16_t BlockAlign =
        Channels * BitsPerSample / 8;
    constexpr std::uint32_t ByteRate =
        SampleRate * BlockAlign;

    const std::uint32_t dataBytes =
        static_cast<std::uint32_t>(
            pcm.size() * sizeof(std::int16_t)
        );
    const std::uint32_t riffBytes =
        36u + dataBytes;

    std::vector<unsigned char> wav(
        static_cast<std::size_t>(44u + dataBytes)
    );

    auto put16 = [&](std::size_t offset, std::uint16_t value)
    {
        wav[offset + 0] =
            static_cast<unsigned char>(value & 0xffu);
        wav[offset + 1] =
            static_cast<unsigned char>((value >> 8u) & 0xffu);
    };

    auto put32 = [&](std::size_t offset, std::uint32_t value)
    {
        wav[offset + 0] =
            static_cast<unsigned char>(value & 0xffu);
        wav[offset + 1] =
            static_cast<unsigned char>((value >> 8u) & 0xffu);
        wav[offset + 2] =
            static_cast<unsigned char>((value >> 16u) & 0xffu);
        wav[offset + 3] =
            static_cast<unsigned char>((value >> 24u) & 0xffu);
    };

    std::memcpy(wav.data() + 0, "RIFF", 4);
    put32(4, riffBytes);
    std::memcpy(wav.data() + 8, "WAVE", 4);
    std::memcpy(wav.data() + 12, "fmt ", 4);
    put32(16, 16u);
    put16(20, 1u);
    put16(22, Channels);
    put32(24, SampleRate);
    put32(28, ByteRate);
    put16(32, BlockAlign);
    put16(34, BitsPerSample);
    std::memcpy(wav.data() + 36, "data", 4);
    put32(40, dataBytes);
    std::memcpy(
        wav.data() + 44,
        pcm.data(),
        static_cast<std::size_t>(dataBytes)
    );

    return wav;
}

const std::vector<unsigned char>& terminalTickWav()
{
    static const std::vector<unsigned char> wav =
        makeTerminalTickWav();
    return wav;
}
#endif
}

AudioEngine& AudioEngine::instance() noexcept
{
    static AudioEngine engine;
    return engine;
}

bool AudioEngine::initialize() noexcept
{
    if (m_initialized)
        return m_available;

    m_initialized = true;

#ifdef _WIN32
    // The first actual playback remains lazy; WinMM is part of Windows.
    m_available = true;
#else
    m_available = false;
#endif

    return m_available;
}

void AudioEngine::shutdown() noexcept
{
#ifdef _WIN32
    PlaySoundW(
        nullptr,
        nullptr,
        0
    );
#endif

    m_available = false;
    m_initialized = false;
}

void AudioEngine::playTerminalTick() noexcept
{
    if (!m_initialized)
        initialize();
    if (!m_available)
        return;

#ifdef _WIN32
    const auto& wav = terminalTickWav();

    // UI sound is intentionally fire-and-forget and non-blocking.
    // The static WAV storage remains alive for the full process lifetime.
    PlaySoundA(
        reinterpret_cast<LPCSTR>(wav.data()),
        nullptr,
        SND_MEMORY |
        SND_ASYNC |
        SND_NODEFAULT
    );
#endif
}

} // namespace audio
