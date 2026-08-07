#pragma once

#include <cstddef>
#include <unordered_map>

#include "src/world/celestial/CelestialSystemRuntime.h"
#include "src/world/celestial/StarAtlasDatabase.h"

namespace world::celestial
{

/*
    Shared demand-driven celestial state resolver.

    Static definitions belong to StarAtlasDatabase. A runtime is created and
    evaluated only when a server subsystem or a client presentation actually
    requests that system at a particular universe time. Server and client
    therefore use the same CelestialSystemRuntime implementation without
    streaming complete planetary snapshots every few frames.
*/
class CelestialRuntimeRegistry
{
public:
    void initialize(const StarAtlasDatabase& atlas);

    const CelestialSystemSnapshot* resolve(
        int systemId,
        double universeTimeSeconds
    ) const;

    std::size_t cachedSystemCount() const noexcept
    {
        return m_runtimes.size();
    }

private:
    const StarAtlasDatabase* m_atlas = nullptr;
    mutable std::unordered_map<int, CelestialSystemRuntime> m_runtimes;
};

} // namespace world::celestial
