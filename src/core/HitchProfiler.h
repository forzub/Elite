#pragma once

#include <chrono>
#include <iostream>
#include <vector>

#include "src/core/RuntimeTrace.h"

namespace core::hitch
{

/*
    Per-frame phase profiler for hitch hunting (ELITE_TRACE_RUNTIME=1 only).
    mark(name) closes the phase that started at the previous mark; endFrame()
    prints the whole phase list when the frame exceeded the threshold, so a
    periodic stall shows which phase grew. Zero cost when tracing is off.
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
};

inline FrameState& state()
{
    static FrameState s;
    return s;
}

inline void beginFrame()
{
    if (!core::runtimeTraceEnabled())
        return;
    auto& s = state();
    s.frameBegin = s.last = std::chrono::steady_clock::now();
    s.marks.clear();
    ++s.frame;
}

inline void mark(const char* name)
{
    if (!core::runtimeTraceEnabled())
        return;
    auto& s = state();
    const auto now = std::chrono::steady_clock::now();
    s.marks.push_back({name, std::chrono::duration<double, std::milli>(now - s.last).count()});
    s.last = now;
}

inline void endFrame(double thresholdMs = 80.0)
{
    if (!core::runtimeTraceEnabled())
        return;
    auto& s = state();
    const double total =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s.frameBegin).count();
    if (total < thresholdMs)
        return;
    std::cerr << "[HITCH] frame=" << s.frame << " total_ms=" << total;
    for (const auto& m : s.marks)
        if (m.ms >= 1.0)
            std::cerr << ' ' << m.name << '=' << m.ms;
    std::cerr << '\n';
}

} // namespace core::hitch
