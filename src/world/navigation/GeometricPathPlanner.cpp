#include "src/world/navigation/GeometricPathPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <tuple>
#include <utility>

#include <glm/gtc/constants.hpp>

#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace world::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;
constexpr double GoldenAngle = 2.39996322972865332;

bool finite3(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double length2 = glm::dot(value, value);
    if (!std::isfinite(length2) || length2 <= Epsilon)
        return fallback;
    return value / std::sqrt(length2);
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

double supportMargin(
    const NavigationObstacle& obstacle,
    const GeometricPathPlannerParams& params
) noexcept
{
    return std::max(
        std::max(0.25, params.supportMarginMeters),
        obstacle.conservativeRadiusMeters() * 0.03
    );
}

glm::dvec3 obstacleToWorld(
    const NavigationObstacle& obstacle,
    const glm::dvec3& local
) noexcept
{
    return obstacle.centerMeters + obstacle.localToWorldBasis * local;
}

glm::dvec3 obstacleToLocal(
    const NavigationObstacle& obstacle,
    const glm::dvec3& world
) noexcept
{
    return glm::transpose(obstacle.localToWorldBasis) *
        (world - obstacle.centerMeters);
}

glm::dvec3 escapePointFromObstacle(
    const glm::dvec3& point,
    const NavigationObstacle& obstacle,
    const GeometricPathPlannerParams& params
) noexcept
{
    const double inflation = navigationObstacleInflationMeters(
        obstacle,
        params.agentRadiusMeters,
        params.additionalClearanceMeters
    );
    const double margin = supportMargin(obstacle, params);

    if (obstacle.shape == NavigationObstacleShape::Box)
    {
        const glm::dvec3 h =
            glm::max(obstacle.halfExtentsMeters, glm::dvec3(0.0)) +
            glm::dvec3(inflation);
        glm::dvec3 local = obstacleToLocal(obstacle, point);
        const glm::dvec3 remaining(
            h.x - std::abs(local.x),
            h.y - std::abs(local.y),
            h.z - std::abs(local.z)
        );

        int axis = 0;
        if (remaining.y < remaining.x)
            axis = 1;
        if (remaining.z < remaining[axis])
            axis = 2;
        local[axis] = (local[axis] >= 0.0 ? 1.0 : -1.0) *
            (h[axis] + margin);
        return obstacleToWorld(obstacle, local);
    }

    if (obstacle.shape == NavigationObstacleShape::Capsule)
    {
        const glm::dvec3 axis = normalizedOr(
            obstacle.localToWorldBasis[2],
            glm::dvec3(0.0, 0.0, 1.0)
        );
        const double halfLength = std::max(0.0, obstacle.capsuleHalfLengthMeters);
        const double along = std::clamp(
            glm::dot(point - obstacle.centerMeters, axis),
            -halfLength,
            halfLength
        );
        const glm::dvec3 axisPoint = obstacle.centerMeters + axis * along;
        const glm::dvec3 radial = normalizedOr(
            point - axisPoint,
            normalizedOr(obstacle.localToWorldBasis[0], glm::dvec3(1.0, 0.0, 0.0))
        );
        return axisPoint + radial *
            (std::max(0.0, obstacle.radiusMeters) + inflation + margin);
    }

    const glm::dvec3 radial = normalizedOr(
        point - obstacle.centerMeters,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    return obstacle.centerMeters + radial *
        (std::max(0.0, obstacle.radiusMeters) + inflation + margin);
}

bool resolveEndpoint(
    const glm::dvec3& original,
    glm::dvec3& resolved,
    const std::vector<NavigationObstacle>& obstacles,
    const GeometricPathPlannerParams& params,
    bool allowEscape,
    bool& escaped
) noexcept
{
    resolved = original;
    escaped = false;

    const std::size_t maxPasses = std::max<std::size_t>(1, obstacles.size() * 2);
    for (std::size_t pass = 0; pass < maxPasses; ++pass)
    {
        bool changed = false;
        for (const auto& obstacle : obstacles)
        {
            if (!pointInsideNavigationObstacle(
                    resolved,
                    obstacle,
                    params.agentRadiusMeters,
                    params.additionalClearanceMeters))
            {
                continue;
            }

            if (!allowEscape)
                return false;

            resolved = escapePointFromObstacle(resolved, obstacle, params);
            escaped = true;
            changed = true;
        }
        if (!changed)
            return true;
    }

    for (const auto& obstacle : obstacles)
    {
        if (pointInsideNavigationObstacle(
                resolved,
                obstacle,
                params.agentRadiusMeters,
                params.additionalClearanceMeters))
        {
            return false;
        }
    }
    return true;
}

std::vector<const NavigationObstacle*> relevantObstacles(
    const GeometricPathRequest& request,
    const glm::dvec3& start,
    const glm::dvec3& goal
)
{
    std::vector<const NavigationObstacle*> ordered;
    ordered.reserve(request.obstacles.size());
    for (const auto& obstacle : request.obstacles)
    {
        if (obstacle.finite())
            ordered.push_back(&obstacle);
    }

    const std::size_t limit = request.params.maxConsideredObstacles;
    if (limit == 0 || ordered.size() <= limit)
        return ordered;

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [&](const NavigationObstacle* a, const NavigationObstacle* b)
        {
            const auto routeClearance = [&](const NavigationObstacle* obstacle)
            {
                const double centerDistance = std::sqrt(
                    pointSegmentDistanceSquared(obstacle->centerMeters, start, goal)
                );
                return std::max(
                    0.0,
                    centerDistance -
                        obstacle->conservativeRadiusMeters() -
                        navigationObstacleInflationMeters(
                            *obstacle,
                            request.params.agentRadiusMeters,
                            request.params.additionalClearanceMeters
                        )
                );
            };
            const double da = routeClearance(a);
            const double db = routeClearance(b);
            if (std::abs(da - db) <= Epsilon)
                return a->id < b->id;
            return da < db;
        }
    );
    ordered.resize(limit);
    return ordered;
}

void addBoxSupportNodes(
    std::vector<glm::dvec3>& nodes,
    const NavigationObstacle& obstacle,
    const GeometricPathPlannerParams& params
)
{
    const double inflation = navigationObstacleInflationMeters(
        obstacle,
        params.agentRadiusMeters,
        params.additionalClearanceMeters
    );
    const double margin = supportMargin(obstacle, params);
    const glm::dvec3 h =
        glm::max(obstacle.halfExtentsMeters, glm::dvec3(0.0)) +
        glm::dvec3(inflation + margin);

    for (int sx : {-1, 1})
    for (int sy : {-1, 1})
    for (int sz : {-1, 1})
    {
        nodes.push_back(obstacleToWorld(
            obstacle,
            glm::dvec3(double(sx) * h.x, double(sy) * h.y, double(sz) * h.z)
        ));
    }

    nodes.push_back(obstacleToWorld(obstacle, glm::dvec3( h.x, 0.0, 0.0)));
    nodes.push_back(obstacleToWorld(obstacle, glm::dvec3(-h.x, 0.0, 0.0)));
    nodes.push_back(obstacleToWorld(obstacle, glm::dvec3(0.0,  h.y, 0.0)));
    nodes.push_back(obstacleToWorld(obstacle, glm::dvec3(0.0, -h.y, 0.0)));
    nodes.push_back(obstacleToWorld(obstacle, glm::dvec3(0.0, 0.0,  h.z)));
    nodes.push_back(obstacleToWorld(obstacle, glm::dvec3(0.0, 0.0, -h.z)));
}

void addSphereSupportNodes(
    std::vector<glm::dvec3>& nodes,
    const NavigationObstacle& obstacle,
    const GeometricPathPlannerParams& params
)
{
    const int samples = std::max(8, params.sphereRadialSamples);
    const double radius = std::max(0.0, obstacle.radiusMeters) +
        navigationObstacleInflationMeters(
            obstacle,
            params.agentRadiusMeters,
            params.additionalClearanceMeters
        ) +
        supportMargin(obstacle, params);

    for (int i = 0; i < samples; ++i)
    {
        const double y = 1.0 - 2.0 * (double(i) + 0.5) / double(samples);
        const double ring = std::sqrt(std::max(0.0, 1.0 - y * y));
        const double angle = GoldenAngle * double(i);
        const glm::dvec3 local(
            std::cos(angle) * ring,
            y,
            std::sin(angle) * ring
        );
        nodes.push_back(obstacle.centerMeters + local * radius);
    }
}

void addCapsuleSupportNodes(
    std::vector<glm::dvec3>& nodes,
    const NavigationObstacle& obstacle,
    const GeometricPathPlannerParams& params
)
{
    const int samples = std::max(8, params.capsuleRadialSamples);
    const double radial = std::max(0.0, obstacle.radiusMeters) +
        navigationObstacleInflationMeters(
            obstacle,
            params.agentRadiusMeters,
            params.additionalClearanceMeters
        ) +
        supportMargin(obstacle, params);
    const double halfLength = std::max(0.0, obstacle.capsuleHalfLengthMeters);

    const glm::dvec3 axis = normalizedOr(
        obstacle.localToWorldBasis[2],
        glm::dvec3(0.0, 0.0, 1.0)
    );
    const glm::dvec3 radialX = normalizedOr(
        obstacle.localToWorldBasis[0],
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 radialY = normalizedOr(
        glm::cross(axis, radialX),
        normalizedOr(obstacle.localToWorldBasis[1], glm::dvec3(0.0, 1.0, 0.0))
    );

    for (double axial : {-halfLength, 0.0, halfLength})
    {
        const glm::dvec3 ringCenter = obstacle.centerMeters + axis * axial;
        for (int i = 0; i < samples; ++i)
        {
            const double angle = glm::two_pi<double>() *
                double(i) / double(samples);
            nodes.push_back(
                ringCenter +
                radialX * (std::cos(angle) * radial) +
                radialY * (std::sin(angle) * radial)
            );
        }
    }

    nodes.push_back(obstacle.centerMeters + axis * (halfLength + radial));
    nodes.push_back(obstacle.centerMeters - axis * (halfLength + radial));
}

std::vector<glm::dvec3> simplifyPath(
    const std::vector<glm::dvec3>& path,
    const std::vector<NavigationObstacle>& obstacles,
    const GeometricPathPlannerParams& params
)
{
    if (!params.simplifyLineOfSight || path.size() <= 2)
        return path;

    std::vector<glm::dvec3> simplified;
    simplified.push_back(path.front());

    std::size_t current = 0;
    while (current + 1 < path.size())
    {
        std::size_t best = current + 1;
        for (std::size_t candidate = path.size() - 1;
             candidate > current + 1;
             --candidate)
        {
            if (segmentClearOfNavigationObstacles(
                    path[current],
                    path[candidate],
                    obstacles,
                    params.agentRadiusMeters,
                    params.additionalClearanceMeters))
            {
                best = candidate;
                break;
            }
        }
        simplified.push_back(path[best]);
        current = best;
    }

    return simplified;
}

double pathLength(const std::vector<glm::dvec3>& points) noexcept
{
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i)
        total += glm::length(points[i] - points[i - 1]);
    return total;
}
}

GeometricPathResult GeometricPathPlanner::plan(const GeometricPathRequest& request)
{
    GeometricPathResult out;
    if (!finite3(request.startMeters) || !finite3(request.goalMeters))
    {
        out.message = "non-finite geometric path endpoint";
        return out;
    }

    std::vector<NavigationObstacle> obstacles;
    const auto relevant = relevantObstacles(request, request.startMeters, request.goalMeters);
    obstacles.reserve(relevant.size());
    for (const auto* obstacle : relevant)
        obstacles.push_back(*obstacle);

    glm::dvec3 safeStart = request.startMeters;
    glm::dvec3 safeGoal = request.goalMeters;
    if (!resolveEndpoint(
            request.startMeters,
            safeStart,
            obstacles,
            request.params,
            request.params.allowStartEscape,
            out.startEscaped))
    {
        out.message = "geometric path start is inside obstacle";
        return out;
    }
    if (!resolveEndpoint(
            request.goalMeters,
            safeGoal,
            obstacles,
            request.params,
            request.params.allowGoalEscape,
            out.goalEscaped))
    {
        out.message = "geometric path goal is inside obstacle";
        return out;
    }

    std::vector<glm::dvec3> routePrefix;
    routePrefix.push_back(request.startMeters);
    if (glm::length(safeStart - request.startMeters) > 1.0e-8)
        routePrefix.push_back(safeStart);

    if (segmentClearOfNavigationObstacles(
            safeStart,
            safeGoal,
            obstacles,
            request.params.agentRadiusMeters,
            request.params.additionalClearanceMeters))
    {
        routePrefix.push_back(safeGoal);
        out.pointsMeters = std::move(routePrefix);
        out.valid = out.pointsMeters.size() >= 2;
        out.obstacleDetourUsed = out.startEscaped || out.goalEscaped;
        out.lengthMeters = pathLength(out.pointsMeters);
        out.message = out.valid ? "direct geometric path" : "geometric path has too few points";
        return out;
    }

    std::vector<glm::dvec3> nodes;
    nodes.reserve(2 + obstacles.size() * 16);
    nodes.push_back(safeStart);
    nodes.push_back(safeGoal);

    for (const auto& obstacle : obstacles)
    {
        switch (obstacle.shape)
        {
            case NavigationObstacleShape::Box:
                addBoxSupportNodes(nodes, obstacle, request.params);
                break;
            case NavigationObstacleShape::Capsule:
                addCapsuleSupportNodes(nodes, obstacle, request.params);
                break;
            case NavigationObstacleShape::Sphere:
            default:
                addSphereSupportNodes(nodes, obstacle, request.params);
                break;
        }
    }

    std::vector<glm::dvec3> filtered;
    filtered.reserve(nodes.size());
    filtered.push_back(nodes[0]);
    filtered.push_back(nodes[1]);
    for (std::size_t i = 2; i < nodes.size(); ++i)
    {
        bool clear = true;
        for (const auto& obstacle : obstacles)
        {
            if (pointInsideNavigationObstacle(
                    nodes[i],
                    obstacle,
                    request.params.agentRadiusMeters,
                    request.params.additionalClearanceMeters))
            {
                clear = false;
                break;
            }
        }
        if (clear)
            filtered.push_back(nodes[i]);
    }
    nodes = std::move(filtered);

    const std::size_t n = nodes.size();
    const double Inf = std::numeric_limits<double>::infinity();
    std::vector<double> g(n, Inf);
    std::vector<std::size_t> parent(n, n);

    struct QueueEntry
    {
        double f = 0.0;
        double g = 0.0;
        std::size_t index = 0;
    };
    struct QueueCompare
    {
        bool operator()(const QueueEntry& a, const QueueEntry& b) const noexcept
        {
            if (std::abs(a.f - b.f) > Epsilon)
                return a.f > b.f;
            if (std::abs(a.g - b.g) > Epsilon)
                return a.g > b.g;
            return a.index > b.index;
        }
    };

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueCompare> open;
    g[0] = 0.0;
    open.push({glm::length(nodes[1] - nodes[0]), 0.0, 0});

    while (!open.empty())
    {
        const QueueEntry current = open.top();
        open.pop();
        if (current.g > g[current.index] + Epsilon)
            continue;
        if (current.index == 1)
            break;

        for (std::size_t next = 0; next < n; ++next)
        {
            if (next == current.index)
                continue;
            if (!segmentClearOfNavigationObstacles(
                    nodes[current.index],
                    nodes[next],
                    obstacles,
                    request.params.agentRadiusMeters,
                    request.params.additionalClearanceMeters))
            {
                continue;
            }

            const double edge = glm::length(nodes[next] - nodes[current.index]);
            const double nextG = current.g + edge;
            if (nextG + Epsilon >= g[next])
                continue;

            g[next] = nextG;
            parent[next] = current.index;
            const double heuristic = glm::length(nodes[1] - nodes[next]);
            open.push({nextG + heuristic, nextG, next});
        }
    }

    if (!std::isfinite(g[1]))
    {
        out.message = "no collision-free geometric path";
        return out;
    }

    std::vector<glm::dvec3> reversed;
    for (std::size_t at = 1;; at = parent[at])
    {
        reversed.push_back(nodes[at]);
        if (at == 0)
            break;
        if (parent[at] >= n)
        {
            out.message = "geometric path parent chain is invalid";
            return out;
        }
    }
    std::reverse(reversed.begin(), reversed.end());
    auto middle = simplifyPath(reversed, obstacles, request.params);

    out.pointsMeters = std::move(routePrefix);
    for (std::size_t i = 1; i < middle.size(); ++i)
        out.pointsMeters.push_back(middle[i]);
    out.valid = out.pointsMeters.size() >= 2;
    out.obstacleDetourUsed = true;
    out.lengthMeters = pathLength(out.pointsMeters);
    out.message = out.valid
        ? "visibility A* geometric path"
        : "geometric path has too few points";
    return out;
}

} // namespace world::navigation
