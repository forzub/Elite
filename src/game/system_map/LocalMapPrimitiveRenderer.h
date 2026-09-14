#pragma once

#include <glm/glm.hpp>

namespace game::system_map
{
void drawLocalMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::vec4& color
);

void drawLocalMapCross(
    const glm::dvec2& point,
    float size,
    const glm::vec4& color
);

void drawLocalMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments,
    const glm::vec4& color
);

// Temporary GL43-B compatibility overloads. Existing Hub/planet callers are
// migrated in subsequent B steps. New code must use the explicit-color forms.
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
