#include "src/world/celestial/CelestialRuntimeRegistry.h"

#include <utility>

namespace world::celestial
{

void CelestialRuntimeRegistry::initialize(
    const StarAtlasDatabase& atlas
)
{
    m_atlas = &atlas;
    m_runtimes.clear();
}

const CelestialSystemSnapshot* CelestialRuntimeRegistry::resolve(
    int systemId,
    double universeTimeSeconds
) const
{
    if (!m_atlas)
        return nullptr;

    auto runtimeIt = m_runtimes.find(systemId);

    if (runtimeIt == m_runtimes.end())
    {
        const CelestialSystemDefinition* definition =
            m_atlas->findSystem(systemId);

        if (!definition)
            return nullptr;

        CelestialSystemRuntime runtime;
        runtime.setSystem(definition);

        runtimeIt =
            m_runtimes.emplace(
                systemId,
                std::move(runtime)
            ).first;
    }

    CelestialSystemRuntime& runtime = runtimeIt->second;
    const CelestialSystemSnapshot& current = runtime.snapshot();

    if (current.bodies.empty() ||
        current.simTimeSeconds != universeTimeSeconds)
    {
        runtime.update(universeTimeSeconds);
    }

    return &runtime.snapshot();
}

} // namespace world::celestial
