#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
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

inline double screenDistanceToSegment(
    const glm::dvec2& point,
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    const glm::dvec2 edge = b - a;
    const double length2 = glm::dot(edge, edge);
    if (length2 <= 1.0e-12)
        return glm::length(point - a);

    const double t = std::clamp(
        glm::dot(point - a, edge) / length2,
        0.0,
        1.0
    );
    return glm::length(point - (a + edge * t));
}

inline double screenDistanceToConvexPolygon(
    const glm::dvec2& point,
    const std::vector<glm::dvec2>& polygon
)
{
    if (polygon.empty())
        return std::numeric_limits<double>::infinity();
    if (screenPointInsideConvexPolygon(point, polygon))
        return 0.0;

    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        best = std::min(
            best,
            screenDistanceToSegment(
                point,
                polygon[i],
                polygon[(i + 1) % polygon.size()]
            )
        );
    }
    return best;
}

} // namespace game::system_map
