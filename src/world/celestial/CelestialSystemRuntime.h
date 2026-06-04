#pragma once

#include "src/world/celestial/CelestialTypes.h"

namespace world::celestial
{

class CelestialSystemRuntime
{
public:
    void setSystem(const CelestialSystemDefinition* definition);

    void update(double simTimeSeconds);

    const CelestialSystemSnapshot& snapshot() const
    {
        return m_snapshot;
    }

private:
    glm::dvec3 computeRelativePositionAu(
        const CelestialBodyDefinition& body,
        double simTimeSeconds
    ) const;

private:
    const CelestialSystemDefinition* m_definition = nullptr;
    CelestialSystemSnapshot m_snapshot;
};

} // namespace world::celestial