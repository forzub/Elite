#pragma once

#include <cstdint>

#include "src/render/celestial/clouds/PlanetaryWeatherMap.h"
#include "src/world/celestial/visual/CelestialEnvironmentProfile.h"

namespace render::celestial::clouds
{

class PlanetaryWeatherMapGenerator
{
public:
    PlanetaryWeatherMap generate(
        const world::celestial::visual::EnvironmentCloudProfile& profile,
        std::uint32_t runtimeSeed,
        int width,
        int height
    ) const;

private:
    static float climateValueAtLatitude(
        float latitudeDeg,
        const world::celestial::visual::
            EnvironmentCloudCirculationProfile& circulation,
        float& outDensityMultiplier,
        float& outFragmentation,
        float& outZonalStretch
    );
};

} // namespace render::celestial::clouds