#pragma once

// The universe time the construction stages of the hubs follow (docs/HUB_CONSTRUCTION.md): the city assembly
// (src/model_asset/HubExpansion.h), the surface settlements' construction sites (SurfaceSettlementLayout.h) and the
// construction traffic read it. The server sets it every step from its UniverseClock (DEV time scale and the daily
// world tick included), the client from its simulation metadata; until the first set: the real clock's universe time
// (UniverseClock::reset - the universe epoch 3098-12-21 = the real 1998-12-21, unix 914198400).

#include <atomic>
#include <chrono>
#include <cmath>

namespace world::time
{
inline std::atomic<double>& constructionUniverseTimeSlot()
{
    static std::atomic<double> t {-1.0};
    return t;
}

inline void setConstructionUniverseTime(double universeSeconds)
{
    constructionUniverseTimeSlot().store(universeSeconds, std::memory_order_relaxed);
}

inline double constructionUniverseTime()
{
    const double t = constructionUniverseTimeSlot().load(std::memory_order_relaxed);
    if (t >= 0.0)
        return t;
    const double unix = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    return unix - 914198400.0;
}

// the construction plans' day: 0 = 3126-01-01 (universe second 853005600)
inline int constructionDay(double universeSeconds)
{
    return static_cast<int>(std::floor((universeSeconds - 853005600.0) / 86400.0));
}
} // namespace world::time
