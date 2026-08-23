#pragma once

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

namespace game::system_map
{

inline bool screenPointInsideConvexPolygon(
    const glm::dvec2& point,
    const std::vector<glm::dvec2>& polygon,
    double epsilon = 1.0e-6
)
{
    if (polygon.size() < 3)
        return false;

    double referenceCross = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const glm::dvec2& a = polygon[i];
        const glm::dvec2& b = polygon[(i + 1) % polygon.size()];
        const glm::dvec2 edge = b - a;
        const glm::dvec2 rel = point - a;
        const double cross = edge.x * rel.y - edge.y * rel.x;

        if (std::abs(cross) <= epsilon)
            continue;

        if (referenceCross == 0.0)
        {
            referenceCross = cross;
            continue;
        }

        if ((referenceCross > 0.0) != (cross > 0.0))
            return false;
    }

    return true;
}

} // namespace game::system_map
