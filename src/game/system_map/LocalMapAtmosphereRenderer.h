#pragma once

#include <glm/glm.hpp>

#include "src/game/system_map/LocalMapEnvironmentStyle.h"

namespace game::system_map
{
void drawLocalMapAtmosphereSoftBand(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const glm::vec4& peakColor,
    double startRadiusFactor,
    double peakRadiusFactor,
    double endRadiusFactor,
    int radialSteps = 24,
    int segments = 256
);

void drawLocalMapAtmosphereStack(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const LocalMapAtmosphereStyle& style,
    bool premultipliedTarget = false
);
}
