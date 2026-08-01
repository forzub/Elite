#pragma once

#include <glm/glm.hpp>

namespace game::system_map
{
void drawLocalMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
);

void drawLocalMapCross(
    const glm::dvec2& point,
    float size
);

void drawLocalMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
);
}
