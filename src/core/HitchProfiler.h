#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <vector>

#include "src/core/RuntimeTrace.h"

namespace core::hitch
{

/*
    Per-frame phase profiler for hitch hunting, always on (a few clock reads
    per frame). mark(name) closes the phase that started at the previous mark;
    endFrame() logs the whole phase list when the frame exceeded the threshold,
    so a periodic stall shows which phase grew: always to
    D:/tmp/elite_hitches.log (one file per session, so a player session
    leaves the evidence), and to stderr with [FrameStats] under
    ELITE_TRACE_RUNTIME=1.
*/
struct Mark
{
    const char* name;
    double ms;
};

struct FrameState
{
    std::chrono::steady_clock::time_point frameBegin;
    std::chrono::steady_clock::time_point last;
    std::vector<Mark> marks;
    long long frame = 0;
    // Rolling window for [FrameStats] (every ~2 s).
    std::chrono::steady_clock::time_point windowBegin;
    int windowFrames = 0;
    int windowSlowFrames = 0;
    double windowSumMs = 0.0;
    double windowMaxMs = 0.0;
};

inline FrameState& state()
{
    static FrameState s;
    return s;
}

inline std::FILE* hitchLog()
{
    static std::FILE* f = [] {
        std::FILE* out = std::fopen("D:/tmp/elite_hitches.log", "w");
        if (out)
        {
            const std::time_t t = std::time(nullptr);
            char stamp[64];
            std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
            std::fprintf(out, "# EliteGame session %s - frames >= 80 ms with their phases (ms)\n", stamp);
        }
        return out;
    }();
    return f;
}

// game time origin of the hitch log (first frame)
inline std::chrono::steady_clock::time_point& sessionBegin()
{
    static std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
    return t;
}

inline void beginFrame()
{
    (void)sessionBegin();
    auto& s = state();
    s.frameBegin = s.last = std::chrono::steady_clock::now();
    s.marks.clear();
    ++s.frame;
}

inline void mark(const char* name)
{
    auto& s = state();
    const auto now = std::chrono::steady_clock::now();
    s.marks.push_back({name, std::chrono::duration<double, std::milli>(now - s.last).count()});
    s.last = now;
}

inline void endFrame(double thresholdMs = 80.0)
{
    auto& s = state();
    const auto now = std::chrono::steady_clock::now();
    const double total = std::chrono::duration<double, std::milli>(now - s.frameBegin).count();
    if (s.windowFrames == 0)
        s.windowBegin = s.frameBegin;
    ++s.windowFrames;
    s.windowSumMs += total;
    s.windowMaxMs = std::max(s.windowMaxMs, total);
    s.windowSlowFrames += total >= 50.0;
    if (std::chrono::duration<double>(now - s.windowBegin).count() >= 2.0)
    {
        if (core::runtimeTraceEnabled())
            std::cerr << "[FrameStats] frames=" << s.windowFrames << " avg_ms=" << s.windowSumMs / s.windowFrames
                  << " max_ms=" << s.windowMaxMs << " over50ms=" << s.windowSlowFrames << '\n';
        s.windowFrames = s.windowSlowFrames = 0;
        s.windowSumMs = s.windowMaxMs = 0.0;
    }
    if (total < thresholdMs)
        return;
    if (std::FILE* f = hitchLog())
    {
        std::fprintf(f, "[HITCH] frame=%lld t_s=%.1f total_ms=%.1f", s.frame,
                     std::chrono::duration<double>(now - sessionBegin()).count(), total);
        for (const auto& m : s.marks)
            if (m.ms >= 1.0)
                std::fprintf(f, " %s=%.1f", m.name, m.ms);
        std::fprintf(f, "\n");
        std::fflush(f);
    }
    if (!core::runtimeTraceEnabled())
        return;
    std::cerr << "[HITCH] frame=" << s.frame << " total_ms=" << total;
    for (const auto& m : s.marks)
        if (m.ms >= 1.0)
            std::cerr << ' ' << m.name << '=' << m.ms;
    std::cerr << '\n';
}

} // namespace core::hitch
