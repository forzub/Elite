#include "src/world/navigation/NavigationObstacleGeometry.h"

#include <algorithm>
#include <cmath>

namespace world::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-12;

bool segmentIntersectsAabb(
    const glm::dvec3& a,
    const glm::dvec3& b,
    const glm::dvec3& halfExtents
) noexcept
{
    const glm::dvec3 d = b - a;
    double tMin = 0.0;
    double tMax = 1.0;

    for (int axis = 0; axis < 3; ++axis)
    {
        const double start = a[axis];
        const double dir = d[axis];
        const double minValue = -halfExtents[axis];
        const double maxValue = halfExtents[axis];

        if (std::abs(dir) <= Epsilon)
        {
            if (start < minValue || start > maxValue)
                return false;
            continue;
        }

        const double inv = 1.0 / dir;
        double t1 = (minValue - start) * inv;
        double t2 = (maxValue - start) * inv;
        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax)
            return false;
    }

    return true;
}

double pointSegmentDistanceSquared(
    const glm::dvec3& point,
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    const glm::dvec3 ab = b - a;
    const double denom = glm::dot(ab, ab);
    if (denom <= Epsilon)
        return glm::dot(point - a, point - a);

    const double t = std::clamp(glm::dot(point - a, ab) / denom, 0.0, 1.0);
    const glm::dvec3 closest = a + ab * t;
    return glm::dot(point - closest, point - closest);
}

double segmentSegmentDistanceSquared(
    const glm::dvec3& p1,
    const glm::dvec3& q1,
    const glm::dvec3& p2,
    const glm::dvec3& q2
) noexcept
{
    const glm::dvec3 d1 = q1 - p1;
    const glm::dvec3 d2 = q2 - p2;
    const glm::dvec3 r = p1 - p2;
    const double a = glm::dot(d1, d1);
    const double e = glm::dot(d2, d2);
    const double f = glm::dot(d2, r);

    double s = 0.0;
    double t = 0.0;

    if (a <= Epsilon && e <= Epsilon)
        return glm::dot(r, r);

    if (a <= Epsilon)
    {
        t = std::clamp(f / e, 0.0, 1.0);
    }
    else
    {
        const double c = glm::dot(d1, r);
        if (e <= Epsilon)
        {
            s = std::clamp(-c / a, 0.0, 1.0);
        }
        else
        {
            const double b = glm::dot(d1, d2);
            const double denom = a * e - b * b;
            if (std::abs(denom) > Epsilon)
                s = std::clamp((b * f - c * e) / denom, 0.0, 1.0);

            t = (b * s + f) / e;
            if (t < 0.0)
            {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            }
            else if (t > 1.0)
            {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }

    const glm::dvec3 c1 = p1 + d1 * s;
    const glm::dvec3 c2 = p2 + d2 * t;
    return glm::dot(c1 - c2, c1 - c2);
}

glm::dvec3 capsuleAxis(const NavigationObstacle& obstacle) noexcept
{
    const glm::dvec3 raw = obstacle.localToWorldBasis[2];
    const double length2 = glm::dot(raw, raw);
    if (!std::isfinite(length2) || length2 <= Epsilon)
        return glm::dvec3(0.0, 0.0, 1.0);
    return raw / std::sqrt(length2);
}
}

double navigationObstacleInflationMeters(
    const NavigationObstacle& obstacle,
    double agentRadiusMeters,
    double additionalClearanceMeters
) noexcept
{
    return std::max(0.0, obstacle.requiredClearanceMeters) +
        std::max(0.0, agentRadiusMeters) +
        std::max(0.0, additionalClearanceMeters);
}

bool pointInsideNavigationObstacle(
    const glm::dvec3& pointMeters,
    const NavigationObstacle& obstacle,
    double agentRadiusMeters,
    double additionalClearanceMeters
) noexcept
{
    const double inflation = navigationObstacleInflationMeters(
        obstacle,
        agentRadiusMeters,
        additionalClearanceMeters
    );

    if (obstacle.shape == NavigationObstacleShape::Box)
    {
        const glm::dvec3 local =
            glm::transpose(obstacle.localToWorldBasis) *
            (pointMeters - obstacle.centerMeters);
        const glm::dvec3 h =
            glm::max(obstacle.halfExtentsMeters, glm::dvec3(0.0)) +
            glm::dvec3(inflation);
        return std::abs(local.x) <= h.x &&
            std::abs(local.y) <= h.y &&
            std::abs(local.z) <= h.z;
    }

    if (obstacle.shape == NavigationObstacleShape::Capsule)
    {
        const glm::dvec3 axis = capsuleAxis(obstacle);
        const double halfLength = std::max(0.0, obstacle.capsuleHalfLengthMeters);
        const glm::dvec3 a = obstacle.centerMeters - axis * halfLength;
        const glm::dvec3 b = obstacle.centerMeters + axis * halfLength;
        const double radius = std::max(0.0, obstacle.radiusMeters) + inflation;
        return pointSegmentDistanceSquared(pointMeters, a, b) <= radius * radius;
    }

    const double radius = std::max(0.0, obstacle.radiusMeters) + inflation;
    return glm::dot(
        pointMeters - obstacle.centerMeters,
        pointMeters - obstacle.centerMeters
    ) <= radius * radius;
}

bool segmentIntersectsNavigationObstacle(
    const glm::dvec3& startMeters,
    const glm::dvec3& endMeters,
    const NavigationObstacle& obstacle,
    double agentRadiusMeters,
    double additionalClearanceMeters
) noexcept
{
    const double inflation = navigationObstacleInflationMeters(
        obstacle,
        agentRadiusMeters,
        additionalClearanceMeters
    );

    if (obstacle.shape == NavigationObstacleShape::Box)
    {
        const glm::dmat3 worldToLocal =
            glm::transpose(obstacle.localToWorldBasis);
        const glm::dvec3 localA = worldToLocal *
            (startMeters - obstacle.centerMeters);
        const glm::dvec3 localB = worldToLocal *
            (endMeters - obstacle.centerMeters);
        const glm::dvec3 h =
            glm::max(obstacle.halfExtentsMeters, glm::dvec3(0.0)) +
            glm::dvec3(inflation);
        return segmentIntersectsAabb(localA, localB, h);
    }

    if (obstacle.shape == NavigationObstacleShape::Capsule)
    {
        const glm::dvec3 axis = capsuleAxis(obstacle);
        const double halfLength = std::max(0.0, obstacle.capsuleHalfLengthMeters);
        const glm::dvec3 a = obstacle.centerMeters - axis * halfLength;
        const glm::dvec3 b = obstacle.centerMeters + axis * halfLength;
        const double radius = std::max(0.0, obstacle.radiusMeters) + inflation;
        return segmentSegmentDistanceSquared(startMeters, endMeters, a, b) <=
            radius * radius;
    }

    const double radius = std::max(0.0, obstacle.radiusMeters) + inflation;
    return pointSegmentDistanceSquared(
        obstacle.centerMeters,
        startMeters,
        endMeters
    ) <= radius * radius;
}

bool segmentClearOfNavigationObstacles(
    const glm::dvec3& startMeters,
    const glm::dvec3& endMeters,
    const std::vector<NavigationObstacle>& obstacles,
    double agentRadiusMeters,
    double additionalClearanceMeters,
    std::string_view ignoredObstacleId
) noexcept
{
    for (const auto& obstacle : obstacles)
    {
        if (!ignoredObstacleId.empty() && obstacle.id == ignoredObstacleId)
            continue;
        if (segmentIntersectsNavigationObstacle(
                startMeters,
                endMeters,
                obstacle,
                agentRadiusMeters,
                additionalClearanceMeters))
        {
            return false;
        }
    }
    return true;
}

} // namespace world::navigation
