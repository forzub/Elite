#include "HudEdgeMapper.h"
#include <limits>
#include <cmath>

// Пересечение луча и отрезка
static bool intersectRaySegment(
    const glm::vec2& rayOrigin,
    const glm::vec2& rayDir,
    const glm::vec2& a,
    const glm::vec2& b,
    float& outT
)
{
    glm::vec2 v1 = rayOrigin - a;
    glm::vec2 v2 = b - a;
    glm::vec2 v3(-rayDir.y, rayDir.x);

    float dot = glm::dot(v2, v3);
    if (std::abs(dot) < 1e-6f)
        return false;

    float t1 = (v2.x * v1.y - v2.y * v1.x) / dot;
    float t2 = glm::dot(v1, v3) / dot;

    if (t1 >= 0.0f && t2 >= 0.0f && t2 <= 1.0f)
    {
        outT = t1;
        return true;
    }

    return false;
}

void HudEdgeMapper::setBoundary(
    const std::vector<glm::vec2>& normalizedBoundary,
    int viewportW,
    int viewportH
)
{
    m_boundaryPx.clear();
    m_boundaryPx.reserve(normalizedBoundary.size());

    for (const auto& p : normalizedBoundary)
    {
        m_boundaryPx.emplace_back(
            p.x * viewportW,
            p.y * viewportH
        );
    }
}

bool HudEdgeMapper::projectDirection(
    const glm::vec2& screenCenter,
    const glm::vec2& direction,
    glm::vec2& outPoint
) const
{
    float bestT = std::numeric_limits<float>::max();
    bool found = false;

    for (size_t i = 0; i < m_boundaryPx.size(); ++i)
    {
        const glm::vec2& a = m_boundaryPx[i];
        const glm::vec2& b = m_boundaryPx[(i + 1) % m_boundaryPx.size()];

        float t;
        if (intersectRaySegment(screenCenter, direction, a, b, t))
        {
            if (t < bestT)
            {
                bestT = t;
                found = true;
            }
        }
    }

    if (found)
        outPoint = screenCenter + direction * bestT;

    return found;
}



bool HudEdgeMapper::isInsideBoundary(const glm::vec2& point) const
{
    // стандартный point-in-polygon (ray casting)
    bool inside = false;
    size_t count = m_boundaryPx.size();

    for (size_t i = 0, j = count - 1; i < count; j = i++)
    {
        const glm::vec2& a = m_boundaryPx[i];
        const glm::vec2& b = m_boundaryPx[j];

        bool intersect =
            ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x);

        if (intersect)
            inside = !inside;
    }

    return inside;
}
