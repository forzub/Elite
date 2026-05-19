#pragma once

#include <cstdio>
#include <iostream>

namespace core
{

inline void disableRuntimeStdoutNoise()
{
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
