#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

#include <glm/glm.hpp>

#include "src/render/celestial/clouds/PlanetaryWeatherMap.h"
#include "src/world/celestial/visual/CelestialEnvironmentProfile.h"

namespace render::celestial::clouds
{

struct CloudAppearanceImage
{
    int width = 0;
    int height = 0;

    std::vector<std::uint8_t> rgba;

    bool valid() const
    {
        return
            width > 0 &&
            height > 0 &&
            rgba.size() ==
                static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height) *
                4u;
    }
};

class CloudAppearanceTextureGenerator
{
public:
    CloudAppearanceImage generate(
        const PlanetaryWeatherMap& weatherMap,
        const world::celestial::visual::
            EnvironmentCloudProfile& profile,
        std::uint32_t seed
    ) const;
};

} // namespace render::celestial::clouds