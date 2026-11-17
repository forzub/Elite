#pragma once

#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace core
{

inline void disableRuntimeStdoutNoise()
{
    // DEV: ELITE_DEV_STDOUT=1 keeps stdout (the docking / route diagnostics print there) - offscreen test runs
    if (const char* keep = std::getenv("ELITE_DEV_STDOUT"); keep && *keep == '1')
    {
        std::cout << std::unitbuf;
        return;
    }
    // Runtime scene must not be throttled by console output.
    // Keep std::cerr alive for fatal/errors, but kill stdout-based noise:
    // std::cout / std::clog / printf.
    std::cout.setstate(std::ios_base::failbit);
    std::clog.setstate(std::ios_base::failbit);

#if defined(_WIN32)
    FILE* sink = nullptr;
    freopen_s(&sink, "NUL", "w", stdout);
#else
    std::freopen("/dev/null", "w", stdout);
#endif
}

} // namespace core
