#pragma once

#include <glm/glm.hpp>

#include "src/render/celestial/ProceduralCloudLayer.h"

namespace render::celestial
{
    class HubBackdropCloudRenderer
    {
    public:
        void render(
            ProceduralCloudLayer& textureSource,
            const glm::dvec2& planetCenterPx,
            double planetRadiusPx,
            double cloudRadiusScale,
            double longitudeOffset,
            double timeSeconds,
            const ProceduralCloudStyle& style
        );
    };
}