#pragma once

#include <cmath>
#include <cstdint>

namespace game::navigation
{

/*
    Immutable time identity for one coherent navigation world state.

    A planner must never combine values resolved at different simulation,
    prediction or presentation epochs. serverTimeSeconds and
    universeTimeSeconds identify the same physical moment on one universe
    timeline revision. sourceTick is provenance: for predicted planning epochs
    it identifies the authoritative seed tick from which the state was derived.

    Client adapters may keep a separate source NavigationPlanningEpoch beside
    the final planning epoch. This type itself remains transport-agnostic.
*/
struct NavigationPlanningEpoch
{
    std::uint64_t sourceTick = 0;
    double serverTimeSeconds = 0.0;
    double universeTimeSeconds = 0.0;
    std::uint64_t universeTimelineRevision = 0;

    bool valid() const noexcept
    {
        return universeTimelineRevision != 0 &&
            std::isfinite(serverTimeSeconds) &&
            std::isfinite(universeTimeSeconds);
    }
};

} // namespace game::navigation
