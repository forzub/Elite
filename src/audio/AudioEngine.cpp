#include "src/audio/AudioEngine.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "miniaudio.h"

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

// Reactor containment siren (author 2026-11-10): a 0.9 s two-tone sweep, repeated by the HUD while the alert lasts.
std::vector<unsigned char> makeSirenWav()
{
    constexpr int SampleRate = 22050;
    constexpr double DurationSeconds = 0.9;
    constexpr int SampleCount = static_cast<int>(SampleRate * DurationSeconds);
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(SampleCount));
    constexpr double Pi = 3.14159265358979323846;
    double phase = 0.0;
    for (int i = 0; i < SampleCount; ++i)
    {
        const double t = static_cast<double>(i) / static_cast<double>(SampleRate);
        const double f = t < 0.45 ? 620.0 + 380.0 * (t / 0.45) : 1000.0 - 380.0 * ((t - 0.45) / 0.45);
        phase += 2.0 * Pi * f / SampleRate;
        const double env = std::min(1.0, t / 0.02) * std::min(1.0, (DurationSeconds - t) / 0.03);
        const double sample = std::clamp((std::sin(phase) + 0.3 * std::sin(2.0 * phase)) * env * 0.18, -1.0, 1.0);
        pcm[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(sample * 32767.0);
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

const std::vector<unsigned char>& sirenWav()
{
    static const std::vector<unsigned char> wav = makeSirenWav();
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

// THE MIXER (miniaudio): one engine, a small pool of voices per effect - a new tick / siren takes a free voice (or the
// oldest one), the others keep playing. The PCM is the generated WAVs' data (no copy).
struct AudioEngine::Backend
{
    ma_engine engine {};
    struct Voice
    {
        ma_audio_buffer buffer {};
        ma_sound sound {};
        bool ready = false;
    };
    struct Effect
    {
        std::vector<Voice> voices;
        std::size_t next = 0;
    };
    Effect tick, siren;
    bool engineReady = false;
};

namespace
{
// a generated WAV (44-byte header, 16-bit PCM) -> its voices
void initEffect(ma_engine& engine, AudioEngine::Backend::Effect& effect, const std::vector<unsigned char>& wav, int voices)
{
    if (wav.size() <= 44)
        return;
    const unsigned channels = static_cast<unsigned>(wav[22] | (wav[23] << 8));
    const unsigned rate = static_cast<unsigned>(wav[24] | (wav[25] << 8) | (wav[26] << 16) | (wav[27] << 24));
    const std::size_t frames = (wav.size() - 44) / (2u * std::max(1u, channels));
    effect.voices.resize(static_cast<std::size_t>(voices));
    for (auto& v : effect.voices)
    {
        ma_audio_buffer_config cfg = ma_audio_buffer_config_init(ma_format_s16, channels, frames, wav.data() + 44, nullptr);
        cfg.sampleRate = rate;
        if (ma_audio_buffer_init(&cfg, &v.buffer) != MA_SUCCESS)
            continue;
        if (ma_sound_init_from_data_source(&engine, &v.buffer, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &v.sound) != MA_SUCCESS)
        {
            ma_audio_buffer_uninit(&v.buffer);
            continue;
        }
        v.ready = true;
    }
}

void playEffect(AudioEngine::Backend::Effect& effect)
{
    if (effect.voices.empty())
        return;
    // a voice that is not playing, else the next in turn (the oldest)
    std::size_t pick = effect.next % effect.voices.size();
    for (std::size_t k = 0; k < effect.voices.size(); ++k)
    {
        const std::size_t i = (effect.next + k) % effect.voices.size();
        if (effect.voices[i].ready && !ma_sound_is_playing(&effect.voices[i].sound))
        {
            pick = i;
            break;
        }
    }
    effect.next = pick + 1;
    auto& v = effect.voices[pick];
    if (!v.ready)
        return;
    ma_sound_stop(&v.sound);
    ma_sound_seek_to_pcm_frame(&v.sound, 0);
    ma_sound_start(&v.sound);
}

bool effectsEnabled()
{
    // author 2026-11-08: "отключи пока все звуковые эфекты" - every sound effect is off until ELITE_SOUND_EFFECTS=1
    static const bool on = [] {
        const char* v = std::getenv("ELITE_SOUND_EFFECTS");
        return v && *v == '1';
    }();
    return on;
}
} // namespace

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
    m_backend = new (std::nothrow) Backend();
    if (!m_backend)
        return m_available = false;
    if (ma_engine_init(nullptr, &m_backend->engine) != MA_SUCCESS)
    {
        delete m_backend;
        m_backend = nullptr;
        return m_available = false;
    }
    m_backend->engineReady = true;
    {
        ma_device* device = ma_engine_get_device(&m_backend->engine);
        std::fprintf(stderr, "[Audio] miniaudio engine: %s, %u Hz, %u ch\n", device ? device->playback.name : "?",
                     ma_engine_get_sample_rate(&m_backend->engine), ma_engine_get_channels(&m_backend->engine));
    }
#ifdef _WIN32
    initEffect(m_backend->engine, m_backend->tick, terminalTickWav(), 6);
    initEffect(m_backend->engine, m_backend->siren, sirenWav(), 2);
#endif
    m_available = true;
    return m_available;
}

void AudioEngine::shutdown() noexcept
{
    if (m_backend)
    {
        for (auto* e : {&m_backend->tick, &m_backend->siren})
            for (auto& v : e->voices)
                if (v.ready)
                {
                    ma_sound_uninit(&v.sound);
                    ma_audio_buffer_uninit(&v.buffer);
                }
        if (m_backend->engineReady)
            ma_engine_uninit(&m_backend->engine);
        delete m_backend;
        m_backend = nullptr;
    }
    m_available = false;
    m_initialized = false;
}

void* AudioEngine::nativeEngine() noexcept
{
    if (!m_initialized)
        initialize();
    return m_available && m_backend ? &m_backend->engine : nullptr;
}

void AudioEngine::playTerminalTick() noexcept
{
    if (!effectsEnabled())
        return;
    if (!m_initialized)
        initialize();
    if (m_available && m_backend)
        playEffect(m_backend->tick);
}

void AudioEngine::playReactorSiren() noexcept
{
    if (!effectsEnabled())
        return;
    if (!m_initialized)
        initialize();
    if (m_available && m_backend)
        playEffect(m_backend->siren);
}

} // namespace audio
